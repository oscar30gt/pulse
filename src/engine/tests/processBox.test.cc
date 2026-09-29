// processBox.test.cc
// Tests for Pulse::Engine::SequentialProcessBox and CombinationalProcessBox using GoogleTest.

#include <gtest/gtest.h>
#include "processBox.h"
#include "wire.h"
#include "signalDrain.h"
#include "constant.h"
#include "signalSource.h"
#include "gates.h"

#include <optional>

using namespace Pulse;
using namespace Pulse::Engine;

// ===========================================================================
// CONSTRUCTION TESTS
// ===========================================================================

TEST(SequentialProcessBoxTest, ConstructionCreatesPorts)
{
    Wire inA(1), outY(1);

    // A sequential process must have at least one wait to avoid infinite loops.
    // Create a minimal valid program: wait(0); (ends immediately after one iteration)
    auto wait = std::make_unique<ProcessInstructionWait>();
    wait->waitTime = 0;

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(wait));

    SequentialProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions));

    EXPECT_TRUE(box.hasInputPort("a"));
    EXPECT_TRUE(box.hasOutputPort("y"));
    EXPECT_FALSE(box.hasPort("nonexistent"));
}

TEST(CombinationalProcessBoxTest, ConstructionCreatesPorts)
{
    Wire inA(1), outY(1);

    CombinationalProcessBox box({ {"a", &inA} }, { {"y", &outY} }, {}, { &inA });

    EXPECT_TRUE(box.hasInputPort("a"));
    EXPECT_TRUE(box.hasOutputPort("y"));
    EXPECT_FALSE(box.hasPort("nonexistent"));
}

// ===========================================================================
// ASSIGNMENT INSTRUCTION TESTS
// ===========================================================================

TEST(CombinationalProcessBoxTest, AssignmentDrivesOutputFromInput)
{
    const bitWidth_t bw = 1;
    Wire inA(bw), outY(bw);

    SignalSource src(bw);
    src.addTarget(&inA);

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(assign));

    Wire trigger(1, LogicVector::FromBool(false));   // what an event probe on `a` would drive
    CombinationalProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions), { &trigger });

    src.drive(LogicVector::FromBool(1));
    box.update(); // first update: every process runs once
    EXPECT_EQ(outY.peek().bit(0), '1');

    src.drive(LogicVector::FromBool(0));
    trigger.drive(LogicVector::FromBool(true)); // next tick: the change is an event of the sensitivity list
    box.update();
    EXPECT_EQ(outY.peek().bit(0), '0');
}

TEST(CombinationalProcessBoxTest, AssignmentWideBits)
{
    const bitWidth_t bw = 8;
    Wire inA(bw), outY(bw);

    SignalSource src(bw);
    src.addTarget(&inA);

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(assign));

    CombinationalProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions), { &inA });

    LogicVector value = LogicVector::FromInt(0xAB);
    src.drive(value);
    box.update();
    EXPECT_EQ(outY.peek(), value);
}

TEST(CombinationalProcessBoxTest, MultipleAssignmentsExecuteInOrder)
{
    const bitWidth_t bw = 1;
    Wire inA(bw), inB(bw), outX(bw), outY(bw);

    SignalSource srcA(bw), srcB(bw);
    srcA.addTarget(&inA);
    srcB.addTarget(&inB);
    srcA.drive(LogicVector::FromBool(1));
    srcB.drive(LogicVector::FromBool(0));
    
    auto assign1 = std::make_unique<ProcessInstructionAssignment>();
    assign1->sourcePort = "a";
    assign1->targetPort = "x";
    
    auto assign2 = std::make_unique<ProcessInstructionAssignment>();
    assign2->sourcePort = "b";
    assign2->targetPort = "y";
    
    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(assign1));
    instructions.push_back(std::move(assign2));
    
    CombinationalProcessBox box({ {"a", &inA}, {"b", &inB} }, { {"x", &outX}, {"y", &outY} }, std::move(instructions), { &inA, &inB });
    box.update();
    
    EXPECT_EQ(outX.peek().bit(0), '1');
    EXPECT_EQ(outY.peek().bit(0), '0');
}

// ===========================================================================
// BRANCH INSTRUCTION TESTS
// ===========================================================================

TEST(CombinationalProcessBoxTest, BranchNotTakenWhenConditionFalse)
{
    // Program: if (cond) skip assign; assign y = a
    // When cond=0 the branch is NOT taken, so the assignment executes.
    const bitWidth_t bw = 1;
    Wire inA(bw), inCond(bw), outY(bw);

    SignalSource srcA(bw), srcCond(bw);
    srcA.addTarget(&inA);
    srcCond.addTarget(&inCond);

    auto branch = std::make_unique<ProcessInstructionBranch>();
    branch->conditionPort = "cond";
    branch->branchLength  = 1; // skip the next instruction if cond=1

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(branch));
    instructions.push_back(std::move(assign));

    CombinationalProcessBox box({ {"a", &inA}, {"cond", &inCond} }, { {"y", &outY} }, std::move(instructions), { &inA, &inCond });

    srcA.drive(LogicVector::FromBool(1));
    srcCond.drive(LogicVector::FromBool(0)); // condition false → do NOT skip
    box.update();

    EXPECT_EQ((bool)outY.peek(), true); // assignment ran
}

TEST(CombinationalProcessBoxTest, BranchTakenSkipsInstructions)
{
    // Program: if (cond) skip assign; assign y = a
    // When cond=1 the branch IS taken, so the assignment is skipped.
    const bitWidth_t bw = 1;
    Wire inA(bw), inCond(bw), outY(bw);

    SignalSource srcA(bw), srcCond(bw);
    srcA.addTarget(&inA);
    srcCond.addTarget(&inCond);

    auto branch = std::make_unique<ProcessInstructionBranch>();
    branch->conditionPort = "cond";
    branch->branchLength  = 1; // skip 1 instruction when cond=0

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(branch));
    instructions.push_back(std::move(assign));

    CombinationalProcessBox box({ {"a", &inA}, {"cond", &inCond} }, { {"y", &outY} }, std::move(instructions), { &inA, &inCond });

    srcA.drive(LogicVector::FromBool(1));
    srcCond.drive(LogicVector::FromBool(0)); // condition false → skip assignment
    box.update();

    EXPECT_EQ(outY.peek().bit(0), 'Z'); // assignment was skipped; output unchanged
}

TEST(CombinationalProcessBoxTest, BranchSkipsMultipleInstructions)
{
    // Program: if (cond) skip 2; assign x=a; assign y=b
    // When cond=1, both assignments are skipped.
    const bitWidth_t bw = 1;
    Wire inA(bw), inB(bw), inCond(bw), outX(bw), outY(bw);

    SignalSource srcA(bw), srcB(bw), srcCond(bw);
    srcA.addTarget(&inA);
    srcB.addTarget(&inB);
    srcCond.addTarget(&inCond);

    auto branch = std::make_unique<ProcessInstructionBranch>();
    branch->conditionPort = "cond";
    branch->branchLength  = 2;

    auto assign1 = std::make_unique<ProcessInstructionAssignment>();
    assign1->sourcePort = "a";
    assign1->targetPort = "x";

    auto assign2 = std::make_unique<ProcessInstructionAssignment>();
    assign2->sourcePort = "b";
    assign2->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(branch));
    instructions.push_back(std::move(assign1));
    instructions.push_back(std::move(assign2));

    CombinationalProcessBox box({ {"a", &inA}, {"b", &inB}, {"cond", &inCond} }, { {"x", &outX}, {"y", &outY} }, std::move(instructions), { &inA, &inB, &inCond });

    srcA.drive(LogicVector::FromBool(1));
    srcB.drive(LogicVector::FromBool(1));
    srcCond.drive(LogicVector::FromBool(0)); // skip both assignments
    box.update();

    EXPECT_EQ(outX.peek().bit(0), 'Z');
    EXPECT_EQ(outY.peek().bit(0), 'Z');
}

TEST(CombinationalProcessBoxTest, ConditionalAssignmentBothPaths)
{
    // Program: if (cond) skip assign; assign y = a
    // Verifies that cond=0 runs the assignment and cond=1 leaves output unchanged.
    const bitWidth_t bw = 1;
    Wire inA(bw), inCond(bw), outY(bw);

    SignalSource srcA(bw), srcCond(bw);
    srcA.addTarget(&inA);
    srcCond.addTarget(&inCond);

    auto branch = std::make_unique<ProcessInstructionBranch>();
    branch->conditionPort = "cond";
    branch->branchLength  = 1;

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(branch));
    instructions.push_back(std::move(assign));

    Wire trigger(1, LogicVector::FromBool(false));
    CombinationalProcessBox box({ {"a", &inA}, {"cond", &inCond} }, { {"y", &outY} }, std::move(instructions), { &trigger });

    // Path 1: cond=1, a=1 → assignment runs → y=1
    srcA.drive(LogicVector::FromBool(1));
    srcCond.drive(LogicVector::FromBool(1));
    box.update();
    EXPECT_EQ((bool)outY.peek(), true);

    // Path 2: cond=0, a=0 → assignment skipped → y stays 1
    srcA.drive(LogicVector::FromBool(0));
    srcCond.drive(LogicVector::FromBool(0));
    trigger.drive(LogicVector::FromBool(true));
    box.update();
    EXPECT_EQ((bool)outY.peek(), true); // output unchanged since assignment was skipped
}

// ===========================================================================
// WAIT INSTRUCTION TESTS  (SequentialProcessBox only)
// ===========================================================================

TEST(SequentialProcessBoxTest, WaitPausesExecution)
{
    // Program: assign y <- 1; wait(3); assign y <- 0; wait infinite
    // The assignment should not execute until 3 extra update() calls have passed.
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), outY(bw);

    Constant zero(&in0, LogicVector::FromBool(0));
    Constant one(&in1, LogicVector::FromBool(1));

    auto wait = std::make_unique<ProcessInstructionWait>();
    wait->waitTime = 2;

    auto assignOne = std::make_unique<ProcessInstructionAssignment>();
    assignOne->sourcePort = "1";
    assignOne->targetPort = "y";

    auto assignZero = std::make_unique<ProcessInstructionAssignment>();
    assignZero->sourcePort = "0";
    assignZero->targetPort = "y";

    auto waitInfinite = std::make_unique<ProcessInstructionWait>();
    waitInfinite->waitTime = (uint64_t)-1;

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(assignOne));
    instructions.push_back(std::move(wait));
    instructions.push_back(std::move(assignZero));
    instructions.push_back(std::move(waitInfinite));

    SequentialProcessBox box({ {"0", &in0}, {"1", &in1} }, { {"y", &outY} }, std::move(instructions));

    box.update(); // assigns y=1 and reaches wait(2)
    EXPECT_EQ((bool)outY.peek(), true); // still waiting

    box.update(); // counter: 2 → 1
    EXPECT_EQ((bool)outY.peek(), true);

    box.update(); // counter: 1 → 0, resumes and runs y=0
    EXPECT_EQ((bool)outY.peek(), false);
}

TEST(SequentialProcessBoxTest, WaitOfOnePausesExactlyOneExtraCycle)
{
    const bitWidth_t bw = 1;
    Wire inA(bw), outY(bw);

    SignalSource src(bw);
    src.addTarget(&inA);
    src.drive(LogicVector::FromBool(1));

    auto wait = std::make_unique<ProcessInstructionWait>();
    wait->waitTime = 1;

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(wait));
    instructions.push_back(std::move(assign));

    SequentialProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions));

    box.update(); // hits wait, counter = 1
    EXPECT_EQ(outY.peek().bit(0), 'Z');

    box.update(); // counter 1 → 0, resumes and runs assignment
    EXPECT_EQ(outY.peek().bit(0), '1');
}

// ===========================================================================
// INSTRUCTION POINTER RESET TESTS
// ===========================================================================

TEST(SequentialProcessBoxTest, InstructionPointerResetsAndLoops)
{
    // SequentialProcessBox loops forever; after the program finishes it restarts
    // from the top on the next update().
    const bitWidth_t bw = 1;
    Wire inA(bw), outY(bw);

    SignalSource src(bw);
    src.addTarget(&inA);

    auto wait = std::make_unique<ProcessInstructionWait>();
    wait->waitTime = 1; // one wait so the loop does not run away in a single update()

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(wait));
    instructions.push_back(std::move(assign));

    SequentialProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions));

    // First iteration
    src.drive(LogicVector::FromBool(1));
    box.update(); // hits wait
    box.update(); // resumes, runs assign → y=1, pointer resets
    EXPECT_EQ((bool)outY.peek(), true);

    // Second iteration: pointer is back at 0, hits wait again
    src.drive(LogicVector::FromBool(0));
    box.update(); // hits wait again
    box.update(); // resumes, runs assign → y=0
    EXPECT_EQ((bool)outY.peek(), false);
}

TEST(CombinationalProcessBoxTest, InstructionPointerResetsOnEachSensChange)
{
    // CombinationalProcessBox re-runs from the start on every sens list change.
    const bitWidth_t bw = 1;
    Wire inA(bw), outY(bw);

    SignalSource src(bw);
    src.addTarget(&inA);

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(assign));

    Wire trigger(1, LogicVector::FromBool(false));
    CombinationalProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions), { &trigger });

    src.drive(LogicVector::FromBool(1));
    box.update();
    EXPECT_EQ((bool)outY.peek(), true);

    src.drive(LogicVector::FromBool(0));
    trigger.drive(LogicVector::FromBool(true));
    box.update(); // triggered → runs from start again
    EXPECT_EQ((bool)outY.peek(), false);
}

// ===========================================================================
// COMBINED INSTRUCTION TESTS
// ===========================================================================

TEST(SequentialProcessBoxTest, WaitThenBranchThenAssign)
{
    // Program: wait(2); if (cond) assign y = a
    const bitWidth_t bw = 1;
    Wire inA(bw), inCond(bw), outY(bw);

    SignalSource srcA(bw), srcCond(bw);
    srcA.addTarget(&inA);
    srcCond.addTarget(&inCond);

    srcA.drive(LogicVector::FromInt(1));
    srcCond.drive(LogicVector::FromBool(false)); // condition false → skip

    auto wait = std::make_unique<ProcessInstructionWait>();
    wait->waitTime = 2;

    auto branch = std::make_unique<ProcessInstructionBranch>();
    branch->conditionPort = "cond";
    branch->branchLength  = 1;

    auto assign = std::make_unique<ProcessInstructionAssignment>();
    assign->sourcePort = "a";
    assign->targetPort = "y";

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(std::move(wait));
    instructions.push_back(std::move(branch));
    instructions.push_back(std::move(assign));

    SequentialProcessBox box({ {"a", &inA}, {"cond", &inCond} }, { {"y", &outY} }, std::move(instructions));

    box.update(); // hits wait
    EXPECT_EQ(outY.peek().bit(0), 'Z');

    box.update(); // counter 2 → 1
    EXPECT_EQ(outY.peek().bit(0), 'Z');

    box.update(); // counter 1 → 0, resumes, assignment skipped due to cond=0
    EXPECT_EQ(outY.peek().bit(0), 'Z');
}

// ===========================================================================
// DRIVE, DEFERRED ASSIGNMENTS AND COMMIT
// ===========================================================================

namespace
{
    std::unique_ptr<ProcessInstructionAssignment> assignment(const std::string& target, const std::string& source, bool deferred)
    {
        auto instruction = std::make_unique<ProcessInstructionAssignment>();
        instruction->targetPort = target;
        instruction->sourcePort = source;
        instruction->deferred = deferred;
        return instruction;
    }

    std::unique_ptr<ProcessInstructionWaitOn> waitOn(std::vector<std::string> triggers, std::string condition = "",
                                                     std::optional<simTime_t> timeout = std::nullopt)
    {
        auto wait = std::make_unique<ProcessInstructionWaitOn>();
        wait->triggers = std::move(triggers);
        wait->conditionPort = std::move(condition);
        wait->hasTimeout = timeout.has_value();
        wait->timeout = timeout.value_or(0);
        return wait;
    }
}

TEST(ProcessBoxTest, DeferredAssignmentIsAppliedOnCommit)
{
    Wire a(4, LogicVector(7)), y(4, LogicVector(0));

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("y", "a", true));
    CombinationalProcessBox box({ {"a", &a} }, { {"y", &y} }, std::move(instructions), { &a });

    box.update();
    EXPECT_EQ(y.peek(), LogicVector(0)); // not before every process ran
    box.commit();
    EXPECT_EQ(y.peek(), LogicVector(7));
}

TEST(ProcessBoxTest, LaterDeferredAssignmentToTheSameSignalWins)
{
    Wire a(4, LogicVector(1)), b(4, LogicVector(2)), y(4, LogicVector(0));

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("y", "a", true));
    instructions.push_back(assignment("y", "b", true));
    CombinationalProcessBox box({ {"a", &a}, {"b", &b} }, { {"y", &y} }, std::move(instructions), { &a });

    box.update();
    box.commit();
    EXPECT_EQ(y.peek(), LogicVector(2));
}

TEST(ProcessBoxTest, DeferredSwapUsesTheOldValues)
{
    // a <= b; b <= a; swaps, because both read the values of before the run.
    Wire a(4, LogicVector(1)), b(4, LogicVector(2));

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("a", "b", true));
    instructions.push_back(assignment("b", "a", true));
    CombinationalProcessBox box({}, { {"a", &a}, {"b", &b} }, std::move(instructions));

    box.update();
    box.commit();
    EXPECT_EQ(a.peek(), LogicVector(2));
    EXPECT_EQ(b.peek(), LogicVector(1));
}

TEST(ProcessBoxTest, ImmediateAssignmentIsSeenByTheNextInstructions)
{
    // v := a (a variable, immediate); y <= not v (deferred). The NOT gate settles as soon as v is driven.
    Wire a(1, LogicVector::FromBool(true)), v(1), notV(1), y(1);
    NOTGate inverter(&v, &notV);

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("v", "a", false));
    instructions.push_back(assignment("y", "nv", true));
    CombinationalProcessBox box({ {"a", &a}, {"nv", &notV} }, { {"v", &v}, {"y", &y} }, std::move(instructions), { &a });

    box.update();
    EXPECT_EQ(v.peek(), LogicVector::FromBool(true));
    box.commit();
    EXPECT_EQ(y.peek(), LogicVector::FromBool(false));
}

TEST(ProcessBoxTest, CommitWithNothingPendingChangesNothing)
{
    Wire a(1, LogicVector::FromBool(true)), y(1, LogicVector::FromBool(false));
    CombinationalProcessBox box({ {"a", &a} }, { {"y", &y} }, {}, { &a });
    box.update();
    box.commit();
    EXPECT_EQ(y.peek(), LogicVector::FromBool(false));
}

// ===========================================================================
// INITIALIZATION AND TRIGGERS
// ===========================================================================

TEST(CombinationalProcessBoxTest, RunsOnceOnItsFirstUpdate)
{
    Wire a(1, LogicVector::FromBool(true)), y(1);
    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("y", "a", false));
    CombinationalProcessBox box({ {"a", &a} }, { {"y", &y} }, std::move(instructions), { &a });

    box.update(); // no event, but every process runs at initialization
    EXPECT_EQ(y.peek(), LogicVector::FromBool(true));
}

TEST(CombinationalProcessBoxTest, RunsOnlyWhenATriggerIsOne)
{
    Wire a(1, LogicVector::FromBool(true)), y(1), t1(1, LogicVector::FromBool(false)), t2(1, LogicVector::FromBool(false));
    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("y", "a", false));
    CombinationalProcessBox box({ {"a", &a} }, { {"y", &y} }, std::move(instructions), { &t1, &t2 });
    box.update();

    a.drive(LogicVector::FromBool(false));
    box.update(); // no trigger: the change of `a` alone does not run the process
    EXPECT_EQ(y.peek(), LogicVector::FromBool(true));

    t2.drive(LogicVector::Unknown());
    box.update(); // an unknown trigger does not run it either
    EXPECT_EQ(y.peek(), LogicVector::FromBool(true));

    t2.drive(LogicVector::FromBool(true));
    box.update();
    EXPECT_EQ(y.peek(), LogicVector::FromBool(false));
}

TEST(CombinationalProcessBoxTest, EmptySensitivityListRunsOnlyOnce)
{
    Wire a(1, LogicVector::FromBool(true)), y(1);
    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("y", "a", false));
    CombinationalProcessBox box({ {"a", &a} }, { {"y", &y} }, std::move(instructions));
    box.update();

    a.drive(LogicVector::FromBool(false));
    box.update();
    EXPECT_EQ(y.peek(), LogicVector::FromBool(true));
}

// ===========================================================================
// CONDITIONS
// ===========================================================================

TEST(CombinationalProcessBoxTest, UnknownOrHighImpedanceConditionIsNotMet)
{
    for (LogicVector condition : { LogicVector::Unknown().range(1), LogicVector::HighZ().range(1) })
    {
        Wire a(1, LogicVector::FromBool(true)), cond(1, condition), y(1, LogicVector::FromBool(false));

        auto branch = std::make_unique<ProcessInstructionBranch>();
        branch->conditionPort = "cond";
        branch->branchLength = 1;

        std::vector<std::unique_ptr<ProcessInstruction>> instructions;
        instructions.push_back(std::move(branch));
        instructions.push_back(assignment("y", "a", false));
        CombinationalProcessBox box({ {"a", &a}, {"cond", &cond} }, { {"y", &y} }, std::move(instructions), { &a });

        box.update();
        EXPECT_EQ(y.peek(), LogicVector::FromBool(false)) << condition.str(1);
    }
}

// ===========================================================================
// WAIT ON / UNTIL / FOR
// ===========================================================================

TEST(SequentialProcessBoxTest, WaitOnResumesOnATrigger)
{
    // wait on s; y := a     (e is what the event probe of s drives)
    Wire e(1, LogicVector::FromBool(false)), a(1, LogicVector::FromBool(true)), y(1, LogicVector::FromBool(false));

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(waitOn({ "e" }));
    instructions.push_back(assignment("y", "a", false));
    SequentialProcessBox box({ {"e", &e}, {"a", &a} }, { {"y", &y} }, std::move(instructions));

    box.update(); // suspends at the wait
    box.update(); // no event
    EXPECT_EQ(y.peek(), LogicVector::FromBool(false));

    e.drive(LogicVector::FromBool(true));
    box.update(); // event on s: resumes, assigns, then waits again
    EXPECT_EQ(y.peek(), LogicVector::FromBool(true));
}

TEST(SequentialProcessBoxTest, WaitUntilNeedsTheConditionOnAnEvent)
{
    // wait on s until c; y := a     (e is what the event probe of s drives)
    Wire e(1, LogicVector::FromBool(false)), c(1, LogicVector::FromBool(false)), a(1, LogicVector::FromBool(true)),
         y(1, LogicVector::FromBool(false));

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(waitOn({ "e" }, "c"));
    instructions.push_back(assignment("y", "a", false));
    SequentialProcessBox box({ {"e", &e}, {"c", &c}, {"a", &a} }, { {"y", &y} }, std::move(instructions));
    box.update();

    e.drive(LogicVector::FromBool(true));
    box.update(); // event, but the condition is false
    EXPECT_EQ(y.peek(), LogicVector::FromBool(false));

    e.drive(LogicVector::FromBool(false));
    c.drive(LogicVector::FromBool(true));
    box.update(); // condition true, but no event on s
    EXPECT_EQ(y.peek(), LogicVector::FromBool(false));

    e.drive(LogicVector::FromBool(true));
    box.update(); // event and condition
    EXPECT_EQ(y.peek(), LogicVector::FromBool(true));
}

TEST(SequentialProcessBoxTest, WaitOnForResumesAfterTheTimeout)
{
    // wait on s for 3; y := a     (s is the trigger: the event probe output of the signal)
    Wire s(1, LogicVector::FromBool(false)), a(1, LogicVector::FromBool(true)), y(1, LogicVector::FromBool(false));

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(waitOn({ "s" }, "", 3));
    instructions.push_back(assignment("y", "a", false));
    instructions.push_back(std::make_unique<ProcessInstructionWaitForever>());
    SequentialProcessBox box({ {"s", &s}, {"a", &a} }, { {"y", &y} }, std::move(instructions));

    box.update(); // suspends, timeout 3
    box.update(); // 2 left
    box.update(); // 1 left
    EXPECT_EQ(y.peek(), LogicVector::FromBool(false));
    box.update(); // elapsed
    EXPECT_EQ(y.peek(), LogicVector::FromBool(true));
}

// ===========================================================================
// PROGRAMS
// ===========================================================================

TEST(ProcessBoxTest, TwoBoxesShareOneProgram)
{
    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("y", "a", false));
    ProcessProgram shared(std::move(instructions));

    Wire a1(1, LogicVector::FromBool(true)), y1(1), a2(1, LogicVector::FromBool(false)), y2(1);
    CombinationalProcessBox first({ {"a", &a1} }, { {"y", &y1} }, shared, { &a1 });
    CombinationalProcessBox second({ {"a", &a2} }, { {"y", &y2} }, shared, { &a2 });

    first.update();
    second.update();
    EXPECT_EQ(y1.peek(), LogicVector::FromBool(true));
    EXPECT_EQ(y2.peek(), LogicVector::FromBool(false));
}

TEST(SequentialProcessBoxTest, LongBodiesRunUntilTheirWait)
{
    Wire a(4, LogicVector(9)), y(4, LogicVector(0));

    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    for (int i = 0; i < 40; ++i)
        instructions.push_back(assignment("y", "a", true));
    instructions.push_back(std::make_unique<ProcessInstructionWaitForever>());
    SequentialProcessBox box({ {"a", &a} }, { {"y", &y} }, std::move(instructions));

    EXPECT_NO_THROW(box.update());
    box.commit();
    EXPECT_EQ(y.peek(), LogicVector(9));
}

TEST(SequentialProcessBoxTest, ABodyWithoutWaitIsAnInfiniteLoop)
{
    Wire a(1), y(1);
    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("y", "a", false));
    SequentialProcessBox box({ {"a", &a} }, { {"y", &y} }, std::move(instructions));

    EXPECT_THROW(box.update(), std::runtime_error);
}

TEST(ProcessBoxTest, AnInstructionOnAnUnknownPortIsRejected)
{
    Wire a(1);
    std::vector<std::unique_ptr<ProcessInstruction>> instructions;
    instructions.push_back(assignment("missing", "a", false));
    EXPECT_THROW(CombinationalProcessBox({ {"a", &a} }, {}, std::move(instructions), { &a }), std::runtime_error);
}
