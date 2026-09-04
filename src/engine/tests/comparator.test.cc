#include <gtest/gtest.h>
#include "comparator.h"
#include "wire.h"
#include "signalSource.h"

using namespace Pulse;
using namespace Pulse::Engine;

// ===========================================================================
// CONSTRUCTION AND PORT TESTS
// ===========================================================================

TEST(ComparatorTest, ConstructionCreatesPorts)
{
    Wire w0(8), w1(8), wOut(1);
    Comparator comp(&w0, &w1, &wOut, CompareOp::Equals, CompareMode::Unsigned);
    EXPECT_TRUE(comp.hasInputPort("in0"));
    EXPECT_TRUE(comp.hasInputPort("in1"));
    EXPECT_TRUE(comp.hasOutputPort("out"));
    EXPECT_FALSE(comp.hasPort("nonexistent"));
}

TEST(ComparatorTest, ConstructionWithDifferentWidths)
{
    Wire w0(16), w1(16), wOut(1);
    Comparator comp(&w0, &w1, &wOut, CompareOp::LessThan, CompareMode::Signed);
    EXPECT_TRUE(comp.hasInputPort("in0"));
    EXPECT_TRUE(comp.hasInputPort("in1"));
    EXPECT_TRUE(comp.hasOutputPort("out"));
}

// ===========================================================================
// EQUALS (==) TESTS
// ===========================================================================

TEST(ComparatorTest, EqualsUnsignedTrueCase)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::Equals, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    LogicVector a = LogicVector::FromInt(0x42);
    src0.drive(a);
    src1.drive(a);

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, EqualsUnsignedFalseCase)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::Equals, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x43));

    EXPECT_EQ(out.peek().bit(0), '0');
}

TEST(ComparatorTest, EqualsSigned)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::Equals, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // Test with negative numbers (-1 in 8-bit signed is 0xFF)
    src0.drive(LogicVector::FromInt(0xFF));
    src1.drive(LogicVector::FromInt(0xFF));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, EqualsZeroValues)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::Equals, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::Zero());
    src1.drive(LogicVector::Zero());

    EXPECT_EQ(out.peek().bit(0), '1');
}

// ===========================================================================
// NOT EQUALS (!=) TESTS
// ===========================================================================

TEST(ComparatorTest, NotEqualsUnsignedTrueCase)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::NotEquals, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x43));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, NotEqualsUnsignedFalseCase)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::NotEquals, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    LogicVector same = LogicVector::FromInt(0x55);
    src0.drive(same);
    src1.drive(same);

    EXPECT_EQ(out.peek().bit(0), '0');
}

// ===========================================================================
// LESS THAN (<) TESTS - UNSIGNED
// ===========================================================================

TEST(ComparatorTest, LessThanUnsignedTrueCase)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x50));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, LessThanUnsignedFalseCaseGreater)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x50));
    src1.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek().bit(0), '0');
}

TEST(ComparatorTest, LessThanUnsignedFalseCaseEqual)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek().bit(0), '0');
}

// ===========================================================================
// LESS THAN (<) TESTS - SIGNED
// ===========================================================================

TEST(ComparatorTest, LessThanSignedNegative)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // -5 (0xFB) < 0 (0x00)
    src0.drive(LogicVector::FromInt(0xFB));
    src1.drive(LogicVector::FromInt(0x00));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, LessThanSignedNegativeVsNegative)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // -10 (0xF6) < -5 (0xFB)
    src0.drive(LogicVector::FromInt(0xF6));
    src1.drive(LogicVector::FromInt(0xFB));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, LessThanSignedPositive)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 10 (0x0A) < 20 (0x14)
    src0.drive(LogicVector::FromInt(0x0A));
    src1.drive(LogicVector::FromInt(0x14));

    EXPECT_EQ(out.peek().bit(0), '1');
}

// ===========================================================================
// LESS THAN OR EQUAL (<=) TESTS
// ===========================================================================

TEST(ComparatorTest, LessThanEqualUnsignedTrue)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThanEqual, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x50));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, LessThanEqualUnsignedEqual)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThanEqual, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, LessThanEqualUnsignedFalse)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThanEqual, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x50));
    src1.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek().bit(0), '0');
}

// ===========================================================================
// GREATER THAN (>) TESTS - UNSIGNED
// ===========================================================================

TEST(ComparatorTest, GreaterThanUnsignedTrue)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThan, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x50));
    src1.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, GreaterThanUnsignedFalse)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThan, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x50));

    EXPECT_EQ(out.peek().bit(0), '0');
}

// ===========================================================================
// GREATER THAN (>) TESTS - SIGNED
// ===========================================================================

TEST(ComparatorTest, GreaterThanSignedNegativeVsPositive)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThan, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 5 (0x05) > -5 (0xFB)
    src0.drive(LogicVector::FromInt(0x05));
    src1.drive(LogicVector::FromInt(0xFB));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, GreaterThanSignedNegativeVsNegative)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThan, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // -5 (0xFB) > -10 (0xF6)
    src0.drive(LogicVector::FromInt(0xFB));
    src1.drive(LogicVector::FromInt(0xF6));

    EXPECT_EQ(out.peek().bit(0), '1');
}

// ===========================================================================
// GREATER THAN OR EQUAL (>=) TESTS
// ===========================================================================

TEST(ComparatorTest, GreaterThanEqualUnsignedTrue)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThanEqual, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x50));
    src1.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, GreaterThanEqualUnsignedEqual)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThanEqual, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, GreaterThanEqualUnsignedFalse)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThanEqual, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x42));
    src1.drive(LogicVector::FromInt(0x50));

    EXPECT_EQ(out.peek().bit(0), '0');
}

// ===========================================================================
// 16-BIT COMPARISONS
// ===========================================================================

TEST(ComparatorTest, 16BitComparison)
{
    const bitWidth_t bw = 16;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    src0.drive(LogicVector::FromInt(0x1234));
    src1.drive(LogicVector::FromInt(0x5678));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, 16BitSignedComparison)
{
    const bitWidth_t bw = 16;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // -1000 (0xFC18) < 1000 (0x03E8)
    src0.drive(LogicVector::FromInt(0xFC18));
    src1.drive(LogicVector::FromInt(0x03E8));

    EXPECT_EQ(out.peek().bit(0), '1');
}

// ===========================================================================
// BOUNDARY CASES
// ===========================================================================

TEST(ComparatorTest, BoundaryMaxUnsigned)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::GreaterThan, CompareMode::Unsigned);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0xFF > 0x00
    src0.drive(LogicVector::FromInt(0xFF));
    src1.drive(LogicVector::FromInt(0x00));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, BoundaryMaxSignedIsNegative)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(1);
    Comparator comp(&in0, &in1, &out, CompareOp::LessThan, CompareMode::Signed);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // 0xFF (-1) < 0x7F (127)
    src0.drive(LogicVector::FromInt(0xFF));
    src1.drive(LogicVector::FromInt(0x7F));

    EXPECT_EQ(out.peek().bit(0), '1');
}

TEST(ComparatorTest, SignedVsUnsignedDifference)
{
    const bitWidth_t bw = 8;
    
    // Unsigned: 0xFF > 0x7F
    Wire in0u(bw), in1u(bw), outu(1);
    Comparator compu(&in0u, &in1u, &outu, CompareOp::GreaterThan, CompareMode::Unsigned);
    SignalSource src0u(bw), src1u(bw);
    src0u.addTarget(&in0u);
    src1u.addTarget(&in1u);
    src0u.drive(LogicVector::FromInt(0xFF));
    src1u.drive(LogicVector::FromInt(0x7F));
    EXPECT_EQ(outu.peek().bit(0), '1');
    
    // Signed: 0xFF (-1) < 0x7F (127)
    Wire in0s(bw), in1s(bw), outs(1);
    Comparator comps(&in0s, &in1s, &outs, CompareOp::LessThan, CompareMode::Signed);
    SignalSource src0s(bw), src1s(bw);
    src0s.addTarget(&in0s);
    src1s.addTarget(&in1s);
    src0s.drive(LogicVector::FromInt(0xFF));
    src1s.drive(LogicVector::FromInt(0x7F));
    EXPECT_EQ(outs.peek().bit(0), '1');
}

// ===========================================================================
// MULTIPLE OPERATIONS ON SAME INPUTS
// ===========================================================================

TEST(ComparatorTest, AllOperationsOnSameInputs)
{
    const bitWidth_t bw = 8;
    LogicVector a = LogicVector::FromInt(0x42);
    LogicVector b = LogicVector::FromInt(0x50);

    Wire in0_eq(bw), in1_eq(bw), out_eq(1);
    Comparator comp_eq(&in0_eq, &in1_eq, &out_eq, CompareOp::Equals, CompareMode::Unsigned);
    SignalSource src0_eq(bw), src1_eq(bw);
    src0_eq.addTarget(&in0_eq);
    src1_eq.addTarget(&in1_eq);
    src0_eq.drive(a);
    src1_eq.drive(b);
    EXPECT_EQ(out_eq.peek().bit(0), '0'); // 0x42 == 0x50? NO

    Wire in0_ne(bw), in1_ne(bw), out_ne(1);
    Comparator comp_ne(&in0_ne, &in1_ne, &out_ne, CompareOp::NotEquals, CompareMode::Unsigned);
    SignalSource src0_ne(bw), src1_ne(bw);
    src0_ne.addTarget(&in0_ne);
    src1_ne.addTarget(&in1_ne);
    src0_ne.drive(a);
    src1_ne.drive(b);
    EXPECT_EQ(out_ne.peek().bit(0), '1'); // 0x42 != 0x50? YES

    Wire in0_lt(bw), in1_lt(bw), out_lt(1);
    Comparator comp_lt(&in0_lt, &in1_lt, &out_lt, CompareOp::LessThan, CompareMode::Unsigned);
    SignalSource src0_lt(bw), src1_lt(bw);
    src0_lt.addTarget(&in0_lt);
    src1_lt.addTarget(&in1_lt);
    src0_lt.drive(a);
    src1_lt.drive(b);
    EXPECT_EQ(out_lt.peek().bit(0), '1'); // 0x42 < 0x50? YES

    Wire in0_gt(bw), in1_gt(bw), out_gt(1);
    Comparator comp_gt(&in0_gt, &in1_gt, &out_gt, CompareOp::GreaterThan, CompareMode::Unsigned);
    SignalSource src0_gt(bw), src1_gt(bw);
    src0_gt.addTarget(&in0_gt);
    src1_gt.addTarget(&in1_gt);
    src0_gt.drive(a);
    src1_gt.drive(b);
    EXPECT_EQ(out_gt.peek().bit(0), '0'); // 0x42 > 0x50? NO
}