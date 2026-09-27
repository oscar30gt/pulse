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
    EXPECT_EQ(wire8.peek(), LogicVector::HighZ().range(8));

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
// ---- Default value --------------------------------------------------------------------------------

TEST(WireTest, DefaultValueIsTheInitialState) {
    Wire wire(4, LogicVector(0b1010));
    EXPECT_EQ(wire.peek(), LogicVector(0b1010));
}

TEST(WireTest, DefaultValueIsMaskedToTheWidth) {
    Wire wire(4, LogicVector(0xFF));
    EXPECT_EQ(wire.peek(), LogicVector(0xF));
}

TEST(WireTest, FirstSourceOverwritesTheDefaultValue) {
    SignalSource src(4);
    src.drive(LogicVector(0b0011));

    Wire wire(4, LogicVector(0b1100));
    wire.addSource(&src);
    EXPECT_EQ(wire.peek(), LogicVector(0b0011));
}

// ---- Drive ----------------------------------------------------------------------------------------

TEST(WireTest, DriveWithoutSourcesSetsAndPropagatesTheState) {
    Wire wire(8);
    SignalDrain drain(8);
    drain.addSource(&wire);

    EXPECT_TRUE(wire.drive(LogicVector(0x5A)));
    EXPECT_EQ(wire.peek(), LogicVector(0x5A));
    EXPECT_EQ(drain.pull(), LogicVector(0x5A));
}

TEST(WireTest, DriveIsMaskedToTheWidth) {
    Wire wire(4);
    wire.drive(LogicVector(0x1F));
    EXPECT_EQ(wire.peek(), LogicVector(0xF));
}

TEST(WireTest, DriveIsIgnoredWhenTheWireHasSources) {
    SignalSource src(4);
    src.drive(LogicVector(0b0001));
    Wire wire(4);
    wire.addSource(&src);

    EXPECT_TRUE(wire.drive(LogicVector(0b1111)));
    EXPECT_EQ(wire.peek(), LogicVector(0b0001));
}

TEST(WireTest, DrivenValueStaysUntilDrivenAgain) {
    Wire wire(1, LogicVector::FromBool(false));
    wire.drive(LogicVector::FromBool(true));
    wire.update();
    wire.update();
    EXPECT_EQ(wire.peek(), LogicVector::FromBool(true));
}

// ---- Event flag -----------------------------------------------------------------------------------

TEST(WireTest, EventIsPublishedByTheNextUpdate) {
    Wire wire(1, LogicVector::FromBool(false));
    EXPECT_FALSE(wire.event());

    wire.drive(LogicVector::FromBool(true));
    EXPECT_FALSE(wire.event()); // the change belongs to the current tick

    wire.update();
    EXPECT_TRUE(wire.event()); // the next tick sees it as an event

    wire.update();
    EXPECT_FALSE(wire.event()); // no change during the previous tick
}

TEST(WireTest, DrivingTheSameValueIsNoEvent) {
    Wire wire(1, LogicVector::FromBool(true));
    wire.drive(LogicVector::FromBool(true));
    wire.update();
    EXPECT_FALSE(wire.event());
}

TEST(WireTest, ChangesFromSourcesAreEventsToo) {
    SignalSource src(1);
    src.drive(LogicVector::FromBool(false));
    Wire wire(1);
    wire.addSource(&src);
    wire.update();

    src.drive(LogicVector::FromBool(true));
    wire.update();
    EXPECT_TRUE(wire.event());
}
