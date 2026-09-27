#include <gtest/gtest.h>

#include "subgraph.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(SubgraphTest, SRFlipFlop)
{
    // Create a subgraph that implements an SR flip-flop using NOR gates
    Blueprint bp;
    bp.addPort("set", true);
    bp.addPort("reset", true);
    bp.addPort("q", false);
    bp.addPort("qNot", false);

    bp.addComponent("nor1", std::make_unique<BinaryGateInstance>("reset", "qNot", "q", BinaryOp::NOR));
    bp.addComponent("nor2", std::make_unique<BinaryGateInstance>("set", "q", "qNot", BinaryOp::NOR));

    // External signals
    Wire set(1), reset(1), q(1), qNot(1);
    SignalSource srcSet(1), srcReset(1);
    srcSet.addTarget(&set);
    srcReset.addTarget(&reset);

    Subgraph flipFlop(bp, { {"set", &set}, {"reset", &reset} }, { {"q", &q}, {"qNot", &qNot} });

    srcSet.drive(LogicVector::FromBool(0));
    srcReset.drive(LogicVector::FromBool(0));

    srcSet.drive(LogicVector::FromBool(1)); // Set
    EXPECT_EQ((bool)q.peek(), true);
    EXPECT_EQ((bool)qNot.peek(), false);
    srcSet.drive(LogicVector::FromBool(0));

    srcReset.drive(LogicVector::FromBool(1)); // Reset
    EXPECT_EQ((bool)q.peek(), false);
    EXPECT_EQ((bool)qNot.peek(), true);
    srcReset.drive(LogicVector::FromBool(0));

    srcSet.drive(LogicVector::FromBool(1)); // Set again
    EXPECT_EQ((bool)q.peek(), true);
    EXPECT_EQ((bool)qNot.peek(), false);
}

TEST(SubgraphTest, RecursiveSubgraphDetection)
{
    // Create a blueprint that references itself to test recursive detection
    Blueprint bp;
    bp.addPort("in", true);
    bp.addPort("out", false);

    // Add a subgraph instance that references the same blueprint
    std::unordered_map<std::string, std::string> portMap = { {"in", "in"}, {"out", "out"} };
    bp.addComponent("selfRef", std::make_unique<SubgraphInstance>(&bp, portMap));

    Wire in(1), out(1);

    // Expect an exception due to recursive subgraph reference
    EXPECT_THROW(
        {
            Subgraph recursiveSubgraph(bp, { {"in", &in} }, { {"out", &out} });
        }, std::runtime_error
    );
}

// ---- Ticks ------------------------------------------------------------------------------------------

namespace
{
    std::vector<std::unique_ptr<ProcessInstruction>> deferredCopy(const std::string& target, const std::string& source)
    {
        auto assignment = std::make_unique<ProcessInstructionAssignment>();
        assignment->targetPort = target;
        assignment->sourcePort = source;
        assignment->deferred = true;

        std::vector<std::unique_ptr<ProcessInstruction>> instructions;
        instructions.push_back(std::move(assignment));
        return instructions;
    }

    LogicVector bit(bool value) { return LogicVector::FromBool(value); }
}

TEST(SubgraphTest, SignalAssignmentsAreCommittedAfterEveryProcessRan)
{
    // p1: q1 <= d;  p2: q2 <= q1;  both run in the first tick. p2 must read q1 before p1's assignment, whatever order
    // the processes run in, so q2 takes the old q1.
    Blueprint bp;
    bp.addSignal("d", 1, bit(false));
    bp.addSignal("q1", 1, bit(true));
    bp.addSignal("q2", 1, bit(false));
    bp.addComponent("p1", std::make_unique<ProcessInstance>(std::vector<std::string>{ "d" }, std::vector<std::string>{ "q1" },
                                                            deferredCopy("q1", "d"), std::vector<std::string>{ "d" }));
    bp.addComponent("p2", std::make_unique<ProcessInstance>(std::vector<std::string>{ "q1" }, std::vector<std::string>{ "q2" },
                                                            deferredCopy("q2", "q1"), std::vector<std::string>{ "q1" }));

    Subgraph graph(bp);
    graph.tick();

    const SubgraphSnapshot snapshot = graph.takeSnapshot();
    EXPECT_EQ(snapshot.wires.at("q1").second, bit(false));
    EXPECT_EQ(snapshot.wires.at("q2").second, bit(true));

    graph.tick(); // q1 changed during the first tick: p2 reacts in the second one
    EXPECT_EQ(graph.takeSnapshot().wires.at("q2").second, bit(false));
}

TEST(SubgraphTest, EventProbesShowTheEventsOfTheTick)
{
    // A process toggles s at the first tick; the probe on s reports the event during the second tick only.
    Blueprint bp;
    bp.addSignal("s", 1, bit(false));
    bp.addSignal("one", 1, bit(true));
    bp.addSignal("e", 1);

    std::vector<std::unique_ptr<ProcessInstruction>> instructions = deferredCopy("s", "one");
    instructions.push_back(std::make_unique<ProcessInstructionWaitForever>());
    bp.addComponent("p", std::make_unique<ProcessInstance>(std::vector<std::string>{ "one" }, std::vector<std::string>{ "s" },
                                                           std::move(instructions)));
    bp.addComponent("probe", std::make_unique<EventProbeInstance>("s", "e"));

    Subgraph graph(bp);
    graph.tick();
    EXPECT_EQ(graph.takeSnapshot().wires.at("e").second, bit(false));
    graph.tick();
    EXPECT_EQ(graph.takeSnapshot().wires.at("e").second, bit(true));
    graph.tick();
    EXPECT_EQ(graph.takeSnapshot().wires.at("e").second, bit(false));
}

TEST(SubgraphTest, WiresStartWithTheirDefaultValue)
{
    Blueprint bp;
    bp.addSignal("v", 8, LogicVector(0xA5));

    Subgraph graph(bp);
    EXPECT_EQ(graph.takeSnapshot().wires.at("v").second, LogicVector(0xA5));
}

// ---- Root and hierarchy -----------------------------------------------------------------------------

TEST(SubgraphTest, RootSubgraphOwnsItsPortWires)
{
    Blueprint bp;
    bp.addPort("a", true, WireInstance{ 4, LogicVector(5) });
    bp.addPort("y", false, WireInstance{ 4 });
    bp.addComponent("join", std::make_unique<JoinInstance>("a", "y"));

    Subgraph graph(bp);
    const SubgraphSnapshot snapshot = graph.takeSnapshot();
    EXPECT_EQ(snapshot.inputs.at("a").first, 4);
    EXPECT_EQ(snapshot.inputs.at("a").second, LogicVector(5));
    EXPECT_EQ(snapshot.outputs.at("y").second, LogicVector(5));
}

TEST(SubgraphTest, ABlueprintCanBeInstantiatedTwice)
{
    // leaf: process y <= a;  top: two leaves.
    Blueprint leaf;
    leaf.addPort("a", true);
    leaf.addPort("y", false);
    leaf.addComponent("p", std::make_unique<ProcessInstance>(std::vector<std::string>{ "a" }, std::vector<std::string>{ "y" },
                                                             deferredCopy("y", "a"), std::vector<std::string>{ "a" }));

    Blueprint top;
    top.addSignal("a1", 1, bit(true));
    top.addSignal("a2", 1, bit(false));
    top.addSignal("y1", 1);
    top.addSignal("y2", 1);
    top.addComponent("u1", std::make_unique<SubgraphInstance>(&leaf, std::unordered_map<std::string, std::string>{ {"a", "a1"}, {"y", "y1"} }));
    top.addComponent("u2", std::make_unique<SubgraphInstance>(&leaf, std::unordered_map<std::string, std::string>{ {"a", "a2"}, {"y", "y2"} }));

    Subgraph graph(top);
    graph.tick();

    const SubgraphSnapshot snapshot = graph.takeSnapshot();
    EXPECT_EQ(snapshot.wires.at("y1").second, bit(true));
    EXPECT_EQ(snapshot.wires.at("y2").second, bit(false));
    EXPECT_EQ(snapshot.subgraphs.size(), 2u);
}

TEST(SubgraphTest, NestedInstancesMayShareALabel)
{
    Blueprint leaf;
    leaf.addPort("a", true);

    Blueprint middle;
    middle.addPort("a", true);
    middle.addComponent("u", std::make_unique<SubgraphInstance>(&leaf, std::unordered_map<std::string, std::string>{ {"a", "a"} }));

    Blueprint top;
    top.addSignal("s", 1);
    top.addComponent("u", std::make_unique<SubgraphInstance>(&middle, std::unordered_map<std::string, std::string>{ {"a", "s"} }));

    EXPECT_NO_THROW(Subgraph graph(top));
}

TEST(SubgraphTest, SnapshotCarriesTheSymbolTable)
{
    Blueprint bp;
    bp.addSignal("n", 32);
    bp.symbols.signals["n"] = SignalSymbol{ DisplayFormat::SignedInteger };

    Subgraph graph(bp);
    const SubgraphSnapshot snapshot = graph.takeSnapshot();
    ASSERT_NE(snapshot.symbols, nullptr);
    ASSERT_NE(snapshot.symbols->find("n"), nullptr);
    EXPECT_EQ(snapshot.symbols->find("n")->format, DisplayFormat::SignedInteger);
}

TEST(SubgraphTest, AMissingWireIsReported)
{
    Blueprint bp;
    bp.addComponent("not", std::make_unique<NotGateInstance>("nowhere", "neither"));
    EXPECT_THROW(Subgraph graph(bp), std::runtime_error);
}
