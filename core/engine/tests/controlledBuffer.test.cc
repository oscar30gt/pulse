#include <gtest/gtest.h>
#include "../include/controlledBuffer.h"
#include "../include/wire.h"
#include "../include/signalSource.h"

using namespace Pulse;
using namespace Pulse::Engine;

// ===========================================================================
// CONSTRUCTION AND PORT TESTS
// ===========================================================================

TEST(ControlledBufferTest, ConstructionCreatesPorts)
{
    Wire in(8), enable(1), out(8);
    ControlledBuffer buf(&in, &enable, &out);
    EXPECT_TRUE(buf.hasInputPort("in"));
    EXPECT_TRUE(buf.hasInputPort("enable"));
    EXPECT_TRUE(buf.hasOutputPort("out"));
    EXPECT_FALSE(buf.hasPort("nonexistent"));
}

TEST(ControlledBufferTest, ConstructionWithDifferentWidths)
{
    Wire in(16), enable(1), out(16);
    ControlledBuffer buf(&in, &enable, &out);
    EXPECT_TRUE(buf.hasInputPort("in"));
    EXPECT_TRUE(buf.hasInputPort("enable"));
    EXPECT_TRUE(buf.hasOutputPort("out"));
}

// ===========================================================================
// ENABLED - PASS THROUGH TESTS
// ===========================================================================

TEST(ControlledBufferTest, EnabledPassesThroughValue)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(1));
    srcIn.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x42).range(bw));
}

TEST(ControlledBufferTest, EnabledPassesThroughZero)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(1));
    srcIn.drive(LogicVector::Zero());

    EXPECT_EQ(out.peek(), LogicVector::Zero().range(bw));
}

TEST(ControlledBufferTest, EnabledPassesThroughAllOnes)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(1));
    srcIn.drive(LogicVector::FromInt(0xFF));

    EXPECT_EQ(out.peek(), LogicVector::FromInt(0xFF).range(bw));
}

TEST(ControlledBufferTest, EnabledSingleBitWidth)
{
    const bitWidth_t bw = 1;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(1));
    srcIn.drive(LogicVector::FromInt(1));

    EXPECT_EQ(out.peek(), LogicVector::FromInt(1).range(bw));
}

TEST(ControlledBufferTest, Enabled16BitPassThrough)
{
    const bitWidth_t bw = 16;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(1));
    srcIn.drive(LogicVector::FromInt(0xBEEF));

    EXPECT_EQ(out.peek(), LogicVector::FromInt(0xBEEF).range(bw));
}

TEST(ControlledBufferTest, Enabled32BitPassThrough)
{
    const bitWidth_t bw = 32;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(1));
    srcIn.drive(LogicVector::FromInt(0xDEADBEEF));

    EXPECT_EQ(out.peek(), LogicVector::FromInt(0xDEADBEEF).range(bw));
}

// ===========================================================================
// DISABLED - HIGH IMPEDANCE TESTS
// ===========================================================================

TEST(ControlledBufferTest, DisabledOutputsHighZ)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(0));
    srcIn.drive(LogicVector::FromInt(0x42));

    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));
}

TEST(ControlledBufferTest, DisabledOutputsHighZRegardlessOfInputValue)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(0));
    srcIn.drive(LogicVector::FromInt(0xFF));

    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));
}

TEST(ControlledBufferTest, DisabledSingleBitWidth)
{
    const bitWidth_t bw = 1;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(0));
    srcIn.drive(LogicVector::FromInt(1));

    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));
}

TEST(ControlledBufferTest, Disabled16BitHighZ)
{
    const bitWidth_t bw = 16;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(0));
    srcIn.drive(LogicVector::FromInt(0xBEEF));

    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));
}

// ===========================================================================
// DYNAMIC / REACTIVE BEHAVIOR TESTS
// ===========================================================================

TEST(ControlledBufferTest, InputChangeWhileEnabledPropagates)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(1));
    srcIn.drive(LogicVector::FromInt(0x10));
    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x10).range(bw));

    srcIn.drive(LogicVector::FromInt(0x20));
    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x20).range(bw));
}

TEST(ControlledBufferTest, InputChangeWhileDisabledDoesNotPropagate)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcEnable.drive(LogicVector::FromInt(0));
    srcIn.drive(LogicVector::FromInt(0x10));
    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));

    srcIn.drive(LogicVector::FromInt(0x20));
    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));
}

TEST(ControlledBufferTest, TogglingEnableSwitchesBetweenValueAndHighZ)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcIn.drive(LogicVector::FromInt(0x77));

    srcEnable.drive(LogicVector::FromInt(1));
    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x77).range(bw));

    srcEnable.drive(LogicVector::FromInt(0));
    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));

    srcEnable.drive(LogicVector::FromInt(1));
    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x77).range(bw));
}

TEST(ControlledBufferTest, EnableAfterInputAlreadyDrivenLatchesCurrentValue)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    // in is driven before enable goes high; buffer starts disabled (Z)
    srcIn.drive(LogicVector::FromInt(0x33));
    srcEnable.drive(LogicVector::FromInt(0));
    EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));

    // once enabled, the already-present input value should show up
    srcEnable.drive(LogicVector::FromInt(1));
    EXPECT_EQ(out.peek(), LogicVector::FromInt(0x33).range(bw));
}

TEST(ControlledBufferTest, RepeatedTogglingEndsInHighZ)
{
    const bitWidth_t bw = 8;
    Wire in(bw), enable(1), out(bw);
    ControlledBuffer buf(&in, &enable, &out);
    SignalSource srcIn(bw), srcEnable(1);
    srcIn.addTarget(&in);
    srcEnable.addTarget(&enable);

    srcIn.drive(LogicVector::FromInt(0x5A));

    for (int i = 0; i < 3; ++i)
    {
        srcEnable.drive(LogicVector::FromInt(1));
        EXPECT_EQ(out.peek(), LogicVector::FromInt(0x5A).range(bw));

        srcEnable.drive(LogicVector::FromInt(0));
        EXPECT_EQ(out.peek(), LogicVector::HighZ().range(bw));
    }
}

// ===========================================================================
// MULTIPLE BUFFERS ON INDEPENDENT WIRES
// ===========================================================================

TEST(ControlledBufferTest, IndependentBuffersDoNotInterfere)
{
    const bitWidth_t bw = 8;

    Wire in0(bw), enable0(1), out0(bw);
    ControlledBuffer buf0(&in0, &enable0, &out0);
    SignalSource srcIn0(bw), srcEnable0(1);
    srcIn0.addTarget(&in0);
    srcEnable0.addTarget(&enable0);

    Wire in1(bw), enable1(1), out1(bw);
    ControlledBuffer buf1(&in1, &enable1, &out1);
    SignalSource srcIn1(bw), srcEnable1(1);
    srcIn1.addTarget(&in1);
    srcEnable1.addTarget(&enable1);

    srcEnable0.drive(LogicVector::FromInt(1));
    srcIn0.drive(LogicVector::FromInt(0x11));

    srcEnable1.drive(LogicVector::FromInt(0));
    srcIn1.drive(LogicVector::FromInt(0x22));

    EXPECT_EQ(out0.peek(), LogicVector::FromInt(0x11).range(bw));
    EXPECT_EQ(out1.peek(), LogicVector::HighZ().range(bw));
}