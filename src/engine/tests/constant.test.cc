// constant_test.cpp
// Exhaustive tests for Pulse::Constant using GoogleTest.

#include <gtest/gtest.h>
#include "../include/constant.h"
#include "../lib/logicVector.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(ConstantTest, ConstructorInitializesState) {
    // Assuming LogicVector can be created from an unsigned integer literal.
    LogicVector initState = LogicVector(0b1100);
    Wire outWire(4);
    Constant c(&outWire, initState);
    EXPECT_EQ(outWire.peek(), initState);
}

TEST(ConstantTest, PeekReturnsConsistentState) {
    LogicVector state = LogicVector(0xFF);
    Wire outWire(64);
    Constant c(&outWire, state);
    EXPECT_EQ(outWire.peek(), state);
    // Constant should remain immutable; peek should always return the same value.
    EXPECT_EQ(outWire.peek(), state);
}
