#include <gtest/gtest.h>

#include "../include/subgraph.h"

using namespace Pulse;
using namespace Pulse::Engine;
using namespace Pulse::Parser;

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