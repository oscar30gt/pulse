// splitter.test.cc
// Comprehensive unit tests for Pulse::Splitter component.

#include <gtest/gtest.h>
#include "splitter.h"
#include "wire.h"
#include "signalSource.h"
#include "signalDrain.h"
#include "constant.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(SplitterTest, ConstructionCreatesPorts)
{
    Wire in(8), out(4);
    Splitter splitter(&in, &out, {7, 4}); // extract upper nibble
    EXPECT_TRUE(splitter.hasInputPort("in"));
    EXPECT_TRUE(splitter.hasOutputPort("out"));
    EXPECT_FALSE(splitter.hasPort("nonexistent"));
}

TEST(SplitterTest, InvalidBitRangeThrows)
{
    Wire in(8), out(4);
    EXPECT_THROW(Splitter(&in, &out, {9, 5}), bit_width_mismatch);
    EXPECT_THROW(Splitter(&in, &out, {8, 0}), bit_width_mismatch);
}

TEST(SplitterTest, OutputWidthMismatchThrows)
{
    Wire in(8), out(3);
    EXPECT_THROW(Splitter(&in, &out, {7, 4}), bit_width_mismatch);
}

TEST(SplitterTest, ExtractsCorrectBitRange)
{
    Wire in(8), out(4);
    Splitter splitter(&in, &out, {7, 4});
    SignalSource src(8);
    src.addTarget(&in);
    src.drive(LogicVector::FromInt(0xBC)); // 10111100
    EXPECT_TRUE(in.notify());
    LogicVector expected = LogicVector::FromInt(0xB); // upper nibble 1011
    EXPECT_EQ(out.peek(), expected);
}

TEST(SplitterTest, ExtractsRangeWhenFirstLessThanSecond)
{
    Wire in(8), out(3);
    Splitter splitter(&in, &out, {0, 2}); // lower 3 bits
    SignalSource src(8);
    src.addTarget(&in);
    src.drive(LogicVector::FromInt(0b11010101)); // LSB = 101
    EXPECT_TRUE(in.notify());
    LogicVector expected = LogicVector::FromInt(0b101);
    EXPECT_EQ(out.peek(), expected);
}

TEST(SplitterTest, VaryingWidthsAndOffsets)
{
    Wire in(12), out(5);
    Splitter splitter(&in, &out, {9,5});
    SignalSource src(12);
    src.addTarget(&in);
    src.drive(LogicVector::FromInt(0x2DE)); // 0010 1101 1110
    EXPECT_TRUE(in.notify());
    LogicVector expected = LogicVector::FromInt(0x16); // bits 9..5 = 10110
    EXPECT_EQ(out.peek(), expected);
}
