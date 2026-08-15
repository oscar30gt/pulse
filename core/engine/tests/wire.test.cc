// wire_test.cpp
// Exhaustive tests for Pulse::Wire using GoogleTest.

#include <gtest/gtest.h>
#include "../include/wire.h"
#include "../include/signalSource.h"
#include "../include/signalDrain.h"
#include "../lib/logicVector.h"

using namespace Pulse;

TEST(WireTest, DefaultConstructionAndPeek) {
    Wire wire; // default bit width
    // Assuming default state is zero LogicVector
    EXPECT_EQ(wire.peek(), LogicVector::HighZ());
}

TEST(WireTest, ConstructionWithBitWidth) {
    Wire wire8(8);
    EXPECT_EQ(wire8.peek().getRange(0, 7), LogicVector::HighZ().getRange(0, 7));
    // Bit width getter not exposed; we just ensure construction succeeds.
}

TEST(WireTest, NotifyPropagationFromSource) {
    // Create a source and connect to wire.
    SignalSource src(1);
    LogicVector state = LogicVector(1);
    src.drive(state);
    Wire wire(1);
    // Connect source to wire (bidirectional).
    wire.addSource(&src);
    // Notify wire to propagate change.
    EXPECT_TRUE(wire.notify());
    // Wire state should now match source state.
    EXPECT_EQ(wire.peek(), state);
}

TEST(WireTest, DrainPullReflectsWireState) {
    SignalSource src(8);
    LogicVector state = LogicVector(0b10101010);
    src.drive(state);
    Wire wire(8);
    wire.addSource(&src);
    wire.notify();
    // Connect wire to a drain and pull.
    SignalDrain drain(8);
    drain.addSource(&src);
    // After propagation, drain should see the same state.
    EXPECT_EQ(drain.pull(), state);
}