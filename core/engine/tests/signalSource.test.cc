// signalSource_test.cpp
// Tests for Pulse::SignalSource using GoogleTest.

#include <gtest/gtest.h>
#include "../include/signalSource.h"
#include "../lib/logicVector.h"

using namespace Pulse::Engine;

TEST(SignalSourceTest, ConstructorInitializesZeroState) {
    SignalSource src(8);
    // Assuming default state is zero.
    EXPECT_EQ(src.peek().range(0, 7), LogicVector::HighZ().range(0, 7));
}

TEST(SignalSourceTest, DriveUpdatesStateAndPeek) {
    SignalSource src(8);
    LogicVector newState = LogicVector(0b10101010);
    // Drive should return true assuming TTL not expired.
    EXPECT_TRUE(src.drive(newState));
    EXPECT_EQ(src.peek(), newState);
}

TEST(SignalSourceTest, DriveWithTTL) {
    SignalSource src(1);
    LogicVector state = LogicVector(1);
    // Use default TTL; expect drive returns true.
    EXPECT_TRUE(src.drive(state));
    EXPECT_EQ(src.peek(), state);
}
