// signalDrain_test.cpp
// Tests for Pulse::SignalDrain using GoogleTest.

#include <gtest/gtest.h>
#include "../include/signalDrain.h"
#include "../include/wire.h"
#include "../include/signalSource.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(SignalDrainTest, BasicConstructionAndPull) {
    // Default constructor creates a drain with default bit width.
    SignalDrain drain;
    // No source connected yet, pull should return default LogicVector.
    LogicVector val = drain.pull();
    // Assuming LogicVector default is zero.
    EXPECT_EQ(val, LogicVector());
}

TEST(SignalDrainTest, ConnectSourceAndPull) {
    SignalDrain drain(8);
    Wire wire(8);
    SignalSource source(8);
    // Set source state to a specific pattern.
    LogicVector state = LogicVector(0b10101010);
    source.drive(state);
    // Connect source to wire.
    wire.addSource(&source);
    // Connect wire to drain.
    drain.addSource(&source);
    // Notify to propagate.
    EXPECT_TRUE(wire.notify());
    // Drain should now see the source state.
    EXPECT_EQ(drain.pull(), state);
}

TEST(SignalDrainTest, NotifyPropagation) {
    SignalDrain drain(1);
    Wire wire(1);
    SignalSource src(1);
    src.drive(LogicVector(1));
    wire.addSource(&src);
    drain.addSource(&src);
    // Notify the wire to propagate the change.
    EXPECT_TRUE(wire.notify());
    // Drain should have updated state.
    EXPECT_EQ(drain.pull(), LogicVector(1));
}
