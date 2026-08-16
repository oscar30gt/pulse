// merger.test.cc
// Comprehensive tests for Pulse::Merger component.

#include <gtest/gtest.h>
#include "../include/merger.h"
#include "../include/wire.h"
#include "../include/signalSource.h"
#include "../include/signalDrain.h"
#include "../include/constant.h"

using namespace Pulse;


TEST(MergerTest, ConstructionCreatesPorts)
{
    Wire in0(4), in1(4), out(8);
    Merger merger(&in0, &in1, &out);
    EXPECT_TRUE(merger.hasInputPort("in0"));
    EXPECT_TRUE(merger.hasInputPort("in1"));
    EXPECT_TRUE(merger.hasOutputPort("out"));
    EXPECT_FALSE(merger.hasPort("nonexistent"));
}

TEST(MergerTest, SimpleConcatenation)
{
    Wire in0(4), in1(4), out(8);
    Merger merger(&in0, &in1, &out);
    SignalSource src0(4), src1(4);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
    src0.drive(LogicVector::FromInt(0b1010)); // low bits
    src1.drive(LogicVector::FromInt(0b1100)); // high bits
    // Propagate changes
    EXPECT_TRUE(in0.notify());
    EXPECT_TRUE(in1.notify());
    // Expected: in1 << 4 | in0
    LogicVector expected = LogicVector::FromInt(0b11001010);
    EXPECT_EQ(out.peek(), expected);
}

TEST(MergerTest, VaryingWidths)
{
    Wire in0(3), in1(5), out(8);
    Merger merger(&in0, &in1, &out);
    SignalSource src0(3), src1(5);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
    src0.drive(LogicVector::FromInt(0b101));
    src1.drive(LogicVector::FromInt(0b11011));
    EXPECT_TRUE(in0.notify());
    EXPECT_TRUE(in1.notify());
    LogicVector expected = LogicVector::FromInt(0b11011101);
    EXPECT_EQ(out.peek(), expected);
}

TEST(MergerTest, ExhaustiveTruthTable)
{
    const bitWidth_t bw = 2;
    Wire in0(bw), in1(bw), out(2 * bw);
    Merger merger(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
    for (int a = 0; a < (1 << bw); ++a)
    {
        for (int b = 0; b < (1 << bw); ++b)
        {
            src0.drive(LogicVector::FromInt(a));
            src1.drive(LogicVector::FromInt(b));
            // Force propagation
            out.notify();
            LogicVector expected = LogicVector::FromInt(b << bw | a);
            EXPECT_EQ(out.peek(), expected);
        }
    }
}

TEST(MergerTest, ChainedMergers)
{
    Wire a(4), b(4), c(4), mid(8), out(12);
    Merger m1(&a, &b, &mid);
    Merger m2(&mid, &c, &out);
    SignalSource sA(4), sB(4), sC(4);
    sA.addTarget(&a);
    sB.addTarget(&b);
    sC.addTarget(&c);
    sA.drive(LogicVector::FromInt(0xA));
    sB.drive(LogicVector::FromInt(0x3));
    sC.drive(LogicVector::FromInt(0xF));
    // Propagation through chain
    out.notify();
    LogicVector expected = (LogicVector::FromInt(0x3 << 4 | 0xA | 0xF << 8));
    EXPECT_EQ(out.peek(), expected);
}

TEST(MergerTest, ConstructionFailsOnWidthMismatch)
{
    Wire in0(4), in1(4), out(7); // sum is 8, out is 7
    EXPECT_THROW({ Merger m(&in0, &in1, &out); }, bit_width_mismatch);
}
