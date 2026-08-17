#include <gtest/gtest.h>
#include "../include/adder.h"
#include "../include/subtractor.h"
#include "../include/multiplicator.h"
#include "../include/wire.h"
#include "../include/signalSource.h"

using namespace Pulse;
using namespace Pulse::Engine;

// ===========================================================================
// ADDER - CONSTRUCTION AND PORT TESTS
// ===========================================================================

TEST(AdderTest, ConstructionCreatesPorts)
{
    Wire w0(8), w1(8), wOut(8);
    Adder add(&w0, &w1, &wOut);
    EXPECT_TRUE(add.hasInputPort("in0"));
    EXPECT_TRUE(add.hasInputPort("in1"));
    EXPECT_TRUE(add.hasOutputPort("out"));
    EXPECT_FALSE(add.hasPort("nonexistent"));
}

TEST(AdderTest, ConstructionWithDifferentWidths)
{
    Wire w0(16), w1(16), wOut(16);
    Adder add(&w0, &w1, &wOut);
    EXPECT_TRUE(add.hasInputPort("in0"));
    EXPECT_TRUE(add.hasInputPort("in1"));
    EXPECT_TRUE(add.hasOutputPort("out"));
}

// ===========================================================================
// ADDER - BASIC ADDITION TESTS
// ===========================================================================

TEST(AdderTest, AddsTwoPositiveValues)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Adder add(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x12));
    src1.drive(LogicVector::FromInt(0x30));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x42);
}

TEST(AdderTest, AddsWithZero)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Adder add(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::Zero());

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x42);
}

TEST(AdderTest, AddsBothZero)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Adder add(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::Zero());
    src1.drive(LogicVector::Zero());

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x00);
}

// ===========================================================================
// ADDER - OVERFLOW / WRAPPING TESTS
// ===========================================================================

TEST(AdderTest, OverflowWrapsAroundWidth)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Adder add(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0xFF + 0x01 wraps to 0x00 (carry is dropped, out is only N bits)
    src0.drive(LogicVector::FromInt(0xFF));
    src1.drive(LogicVector::FromInt(0x01));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x00);
}

TEST(AdderTest, MaxValuesWrap)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Adder add(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0xFF + 0xFF = 0x1FE, wraps to 0xFE
    src0.drive(LogicVector::FromInt(0xFF));
    src1.drive(LogicVector::FromInt(0xFF));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0xFE);
}

// ===========================================================================
// ADDER - 16-BIT ADDITION
// ===========================================================================

TEST(AdderTest, 16BitAddition)
{
    const bitWidth_t bw = 16;
    Wire in0(bw), in1(bw), out(bw);
    Adder add(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x1234));
    src1.drive(LogicVector::FromInt(0x0111));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x1345);
}

// ===========================================================================
// SUBTRACTOR - CONSTRUCTION AND PORT TESTS
// ===========================================================================

TEST(SubtractorTest, ConstructionCreatesPorts)
{
    Wire w0(8), w1(8), wOut(8);
    Subtractor sub(&w0, &w1, &wOut);
    EXPECT_TRUE(sub.hasInputPort("in0"));
    EXPECT_TRUE(sub.hasInputPort("in1"));
    EXPECT_TRUE(sub.hasOutputPort("out"));
    EXPECT_FALSE(sub.hasPort("nonexistent"));
}

TEST(SubtractorTest, ConstructionWithDifferentWidths)
{
    Wire w0(16), w1(16), wOut(16);
    Subtractor sub(&w0, &w1, &wOut);
    EXPECT_TRUE(sub.hasInputPort("in0"));
    EXPECT_TRUE(sub.hasInputPort("in1"));
    EXPECT_TRUE(sub.hasOutputPort("out"));
}

// ===========================================================================
// SUBTRACTOR - BASIC SUBTRACTION TESTS
// ===========================================================================

TEST(SubtractorTest, SubtractsTwoPositiveValues)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Subtractor sub(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x50));
    src1.drive(LogicVector::FromInt(0x10));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x40);
}

TEST(SubtractorTest, SubtractsZero)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Subtractor sub(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::Zero());

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x42);
}

TEST(SubtractorTest, SubtractsFromItselfIsZero)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Subtractor sub(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    LogicVector a = LogicVector::FromInt(0x77);
    src0.drive(a);
    src1.drive(a);

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x00);
}

// ===========================================================================
// SUBTRACTOR - UNDERFLOW / WRAPPING TESTS
// ===========================================================================

TEST(SubtractorTest, UnderflowWrapsAroundWidth)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Subtractor sub(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0x00 - 0x01 wraps to 0xFF
    src0.drive(LogicVector::Zero());
    src1.drive(LogicVector::FromInt(0x01));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0xFF);
}

TEST(SubtractorTest, UnderflowLargeDifference)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    Subtractor sub(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0x10 - 0x20 wraps to 0xF0
    src0.drive(LogicVector::FromInt(0x10));
    src1.drive(LogicVector::FromInt(0x20));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0xF0);
}

// ===========================================================================
// SUBTRACTOR - 16-BIT SUBTRACTION
// ===========================================================================

TEST(SubtractorTest, 16BitSubtraction)
{
    const bitWidth_t bw = 16;
    Wire in0(bw), in1(bw), out(bw);
    Subtractor sub(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x5678));
    src1.drive(LogicVector::FromInt(0x1234));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x4444);
}

// ===========================================================================
// MULTIPLICATOR - CONSTRUCTION AND PORT TESTS
// ===========================================================================

TEST(MultiplicatorTest, ConstructionCreatesPorts)
{
    Wire w0(8), w1(8), wOut(16);
    Multiplicator mul(&w0, &w1, &wOut);
    EXPECT_TRUE(mul.hasInputPort("in0"));
    EXPECT_TRUE(mul.hasInputPort("in1"));
    EXPECT_TRUE(mul.hasOutputPort("out"));
    EXPECT_FALSE(mul.hasPort("nonexistent"));
}

TEST(MultiplicatorTest, ConstructionWithDifferentInputWidths)
{
    Wire w0(4), w1(8), wOut(12);
    Multiplicator mul(&w0, &w1, &wOut);
    EXPECT_TRUE(mul.hasInputPort("in0"));
    EXPECT_TRUE(mul.hasInputPort("in1"));
    EXPECT_TRUE(mul.hasOutputPort("out"));
}

// ===========================================================================
// MULTIPLICATOR - BASIC MULTIPLICATION TESTS
// ===========================================================================

TEST(MultiplicatorTest, MultipliesTwoPositiveValues)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(2 * bw);
    Multiplicator mul(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x06));
    src1.drive(LogicVector::FromInt(0x07));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x2A); // 6 * 7 = 42
}

TEST(MultiplicatorTest, MultipliesByZero)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(2 * bw);
    Multiplicator mul(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::Zero());

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x00);
}

TEST(MultiplicatorTest, MultipliesByOne)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(2 * bw);
    Multiplicator mul(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x01));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x42);
}

// ===========================================================================
// MULTIPLICATOR - BOUNDARY / WIDTH-GROWTH CASES
// ===========================================================================

TEST(MultiplicatorTest, MaxValuesNeedFullOutputWidth)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(2 * bw);
    Multiplicator mul(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0xFF * 0xFF = 0xFE01, requires the full 16 output bits (N + M)
    src0.drive(LogicVector::FromInt(0xFF));
    src1.drive(LogicVector::FromInt(0xFF));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0xFE01);
}

TEST(MultiplicatorTest, DifferentInputWidths)
{
    Wire in0(4), in1(8), out(12);
    Multiplicator mul(&in0, &in1, &out);
    SignalSource src0(4), src1(8);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0xF (4 bits) * 0xFF (8 bits) = 0xEF1, requires the full 12 output bits
    src0.drive(LogicVector::FromInt(0xF));
    src1.drive(LogicVector::FromInt(0xFF));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0xEF1);
}

// ===========================================================================
// MULTIPLICATOR - 16-BIT INPUTS
// ===========================================================================

TEST(MultiplicatorTest, 16BitMultiplication)
{
    const bitWidth_t bw = 16;
    Wire in0(bw), in1(bw), out(2 * bw);
    Multiplicator mul(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x1234));
    src1.drive(LogicVector::FromInt(0x0002));

    EXPECT_EQ(static_cast<uint64_t>(out.peek()), 0x2468);
}