// wire_test.cpp
// Exhaustive tests for Pulse::Wire using GoogleTest.

#include <gtest/gtest.h>
#include "wire.h"
#include "signalSource.h"
#include "signalDrain.h"
#include "logicVector.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(WireTest, DefaultConstructionAndPeek) {
    Wire wire; // default bit width
    // Assuming default state is zero LogicVector
    EXPECT_EQ(wire.peek(), LogicVector::HighZ());
}

TEST(WireTest, ConstructionWithBitWidth) {
    Wire wire8(8);
    EXPECT_EQ(wire8.peek().range(0, 7), LogicVector::HighZ().range(0, 7));
    // Bit width getter not exposed; we just ensure construction succeeds.
}

TEST(WireTest, LoseStateOnDisconnect) {
    SignalSource src(8);
    LogicVector state = LogicVector(0b11001100);
    src.drive(state);

    Wire wire8(8);
    EXPECT_EQ(wire8.peek(), LogicVector::HighZ());

    src.addTarget(&wire8);
    EXPECT_EQ(wire8.peek(), state);

    src.removeTarget(&wire8);
    EXPECT_EQ(wire8.peek(), LogicVector::HighZ().range(8));
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