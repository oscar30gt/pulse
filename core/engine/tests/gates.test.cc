// gates_test.cpp
// Tests for Pulse::ANDGate using GoogleTest.

#include <gtest/gtest.h>
#include "../include/gates.h"
#include "../include/wire.h"
#include "../include/signalDrain.h"
#include "../include/signalSource.h"

using namespace Pulse;

TEST(ANDGateTest, ConstructionCreatesPorts) {
    ANDGate gate;
    // ANDGate inherits Component, so ports "in0", "in1", "out" are expected.
    EXPECT_TRUE(gate.hasInputPort("in0"));
    EXPECT_TRUE(gate.hasInputPort("in1"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
    EXPECT_FALSE(gate.hasPort("nonexistent"));
}

TEST(ANDGateTest, ConnectInputSignalsAndRecalculate) {
    ANDGate gate(1);
    Wire w0(1), w1(1), wOut(1);
    // Connect wires to gate ports
    gate.connect("in0", w0);
    gate.connect("in1", w1);
    gate.connect("out", wOut);
    // Set input signals via SignalSource
    SignalSource src0(1), src1(1);
    src0.drive(LogicVector(0));
    src1.drive(LogicVector(1));
    // Connect sources to wires
    w0.addSource(&src0);
    w1.addSource(&src1);
    // Simulate propagation (notify chain)
    EXPECT_TRUE(w0.notify());
    EXPECT_TRUE(w1.notify());
    // Now the AND gate should recalculate and set output to 0 (0 AND 1)
    // Since internal recalculate is private, we verify via output wire state.
    // Assuming Wire::peek returns LogicVector.
    // The output wire should have logic 0 after propagation.
    // Trigger gate recalculation by notifying input wires.
    EXPECT_TRUE(gate.getSignal("in0")->notify());
    EXPECT_TRUE(gate.getSignal("in1")->notify());
    // Output wire should reflect AND result.
    // Note: This test assumes that Wire::peek returns LogicVector and that
    // logical 0 is represented by LogicVector(0).
    EXPECT_EQ(wOut.peek(), LogicVector(0));
}
  
// Additional tests could include varying all combinations of inputs.
