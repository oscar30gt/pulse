// processBox_test.cc
// Tests for Pulse::Engine::SequentialProcessBox and CombinationalProcessBox using GoogleTest.

#include <gtest/gtest.h>
#include "processBox.h"
#include "wire.h"
#include "signalDrain.h"
#include "constant.h"
#include "signalSource.h"

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

    CombinationalProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions), { &inA });

    src.drive(LogicVector::FromBool(1)); // triggers sens list change
    box.update();
    EXPECT_EQ(outY.peek().bit(0), '1');

    src.drive(LogicVector::FromBool(0)); // triggers sens list change
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

    CombinationalProcessBox box({ {"a", &inA}, {"cond", &inCond} }, { {"y", &outY} }, std::move(instructions), { &inA, &inCond });

    // Path 1: cond=1, a=1 → assignment runs → y=1
    srcA.drive(LogicVector::FromBool(1));
    srcCond.drive(LogicVector::FromBool(1));
    box.update();
    EXPECT_EQ((bool)outY.peek(), true);

    // Path 2: cond=0, a=0 → assignment skipped → y stays 1
    srcA.drive(LogicVector::FromBool(0));
    srcCond.drive(LogicVector::FromBool(0));
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

    CombinationalProcessBox box({ {"a", &inA} }, { {"y", &outY} }, std::move(instructions), { &inA });

    src.drive(LogicVector::FromBool(1));
    box.update();
    EXPECT_EQ((bool)outY.peek(), true);

    src.drive(LogicVector::FromBool(0));
    box.update(); // sens list changed → runs from start again
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