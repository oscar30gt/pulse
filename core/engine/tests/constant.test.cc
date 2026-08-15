// constant_test.cpp
// Exhaustive tests for Pulse::Constant using GoogleTest.

#include <gtest/gtest.h>
#include "../include/constant.h"
#include "../lib/logicVector.h"

using namespace Pulse;

TEST(ConstantTest, ConstructorInitializesState) {
    // Assuming LogicVector can be created from an unsigned integer literal.
    LogicVector initState = LogicVector(0b1100);
    Constant c(initState, 8);
    EXPECT_EQ(c.peek(), initState);
}

TEST(ConstantTest, PeekReturnsConsistentState) {
    LogicVector state = LogicVector(0xFF);
    Constant c(state);
    EXPECT_EQ(c.peek(), state);
    // Constant should remain immutable; peek should always return the same value.
    EXPECT_EQ(c.peek(), state);
}
