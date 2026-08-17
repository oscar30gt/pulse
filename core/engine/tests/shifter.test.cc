// shifter.test.cc
// Comprehensive unit tests for Pulse::Shifter component.

#include <gtest/gtest.h>
#include "../include/shifter.h"
#include "../include/wire.h"
#include "../include/signalSource.h"
#include "../include/constant.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(ShifterTest, ConstructionCreatesPorts)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalLeft);
    EXPECT_TRUE(shifter.hasInputPort("in"));
    EXPECT_TRUE(shifter.hasInputPort("shamt"));
    EXPECT_TRUE(shifter.hasOutputPort("out"));
    EXPECT_FALSE(shifter.hasPort("nonexistent"));
}

TEST(ShifterTest, DefaultOperationIsLeft)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out); // default should be Left
    EXPECT_TRUE(shifter.hasOutputPort("out"));
}

TEST(ShifterTest, LogicalLeftShift)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalLeft);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x03));  // 0000 0011
    shamtSrc.drive(LogicVector::FromInt(2));   // shift by 2
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0x0C); // 0000 1100
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, LogicalRightShift)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalRight);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x30));  // 0011 0000
    shamtSrc.drive(LogicVector::FromInt(2));   // shift by 2
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0x0C); // 0000 1100
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, ArithmeticRightShiftPreservesSign)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::ArithmeticRight);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x80));  // 1000 0000 (negative in 8-bit signed)
    shamtSrc.drive(LogicVector::FromInt(2));   // shift by 2
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0xE0); // 1110 0000 (sign-extended)
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, ArithmeticRightShiftPositive)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::ArithmeticRight);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x78));  // 0111 1000 (positive)
    shamtSrc.drive(LogicVector::FromInt(2));   // shift by 2
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0x1E); // 0001 1110
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, RotateLeft)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::RotateLeft);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x81));  // 1000 0001
    shamtSrc.drive(LogicVector::FromInt(1));   // rotate by 1
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0x03); // 0000 0011
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, RotateRight)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::RotateRight);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x81));  // 1000 0001
    shamtSrc.drive(LogicVector::FromInt(1));   // rotate by 1
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0xC0); // 1100 0000
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, ShiftByZero)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalLeft);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0xAB));
    shamtSrc.drive(LogicVector::FromInt(0));    // shift by 0
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0xAB); // unchanged
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, LeftShiftByFullWidth)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalLeft);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x01));
    shamtSrc.drive(LogicVector::FromInt(8));    // shift by width
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0x00); // all bits shifted out
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, RightShiftByFullWidth)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalRight);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x80));
    shamtSrc.drive(LogicVector::FromInt(8));    // shift by width
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0x00); // all bits shifted out
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, Rotate8BitsByFullAmount)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::RotateLeft);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    LogicVector pattern = LogicVector::FromInt(0xA5); // 1010 0101
    inSrc.drive(pattern);
    shamtSrc.drive(LogicVector::FromInt(8));    // rotate by 8 (full rotation)
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    EXPECT_EQ(out.peek(), pattern); // should return to original
}

TEST(ShifterTest, VaryingWidths16Bit)
{
    Wire in(16), shamt(6), out(16);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalLeft);
    
    SignalSource inSrc(16), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    inSrc.drive(LogicVector::FromInt(0x00FF));  // 0000 0000 1111 1111
    shamtSrc.drive(LogicVector::FromInt(4));     // shift by 4
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    
    LogicVector expected = LogicVector::FromInt(0x0FF0); // 0000 1111 1111 0000
    EXPECT_EQ(out.peek(), expected);
}

TEST(ShifterTest, MultipleShiftsSequential)
{
    Wire in(8), shamt(6), out(8);
    Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalLeft);
    
    SignalSource inSrc(8), shamtSrc(6);
    inSrc.addTarget(&in);
    shamtSrc.addTarget(&shamt);
    
    // First shift
    inSrc.drive(LogicVector::FromInt(0x01));
    shamtSrc.drive(LogicVector::FromInt(1));
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x02));
    
    // Second shift
    inSrc.drive(LogicVector::FromInt(0x02));
    shamtSrc.drive(LogicVector::FromInt(2));
    EXPECT_TRUE(in.notify());
    EXPECT_TRUE(shamt.notify());
    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x08));
}

TEST(ShifterTest, AllOperationsWithSameInput)
{
    Wire in(8), shamt(6), out(8);
    LogicVector input = LogicVector::FromInt(0x42); // 0100 0010
    
    // Test each operation type
    {
        Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalLeft);
        SignalSource inSrc(8), shamtSrc(6);
        inSrc.addTarget(&in);
        shamtSrc.addTarget(&shamt);
        inSrc.drive(input);
        shamtSrc.drive(LogicVector::FromInt(1));
        in.notify();
        shamt.notify();
        EXPECT_EQ(out.peek(), LogicVector::FromInt(0x84)); // left shift by 1
    }
    
    {
        Shifter shifter(&in, &shamt, &out, ShiftOp::LogicalRight);
        SignalSource inSrc(8), shamtSrc(6);
        inSrc.addTarget(&in);
        shamtSrc.addTarget(&shamt);
        inSrc.drive(input);
        shamtSrc.drive(LogicVector::FromInt(1));
        in.notify();
        shamt.notify();
        EXPECT_EQ(out.peek(), LogicVector::FromInt(0x21)); // right shift by 1
    }
}