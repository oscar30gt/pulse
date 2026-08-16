// gates_test.cpp
// Tests for Pulse::ANDGate using GoogleTest.

#include <gtest/gtest.h>
#include "../include/gates.h"
#include "../include/wire.h"
#include "../include/signalDrain.h"
#include "../include/signalSource.h"
#include "../include/constant.h"

using namespace Pulse;

TEST(ANDGateTest, ConstructionCreatesPorts)
{
    Wire w0(1), w1(1), wOut(1);
    ANDGate gate(&w0, &w1, &wOut);
    // ANDGate inherits Component, so ports "in0", "in1", "out" are expected.
    EXPECT_TRUE(gate.hasInputPort("in0"));
    EXPECT_TRUE(gate.hasInputPort("in1"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
    EXPECT_FALSE(gate.hasPort("nonexistent"));
}

TEST(ANDGateTest, ConnectInputSignalsAndRecalculate)
{
    Wire w0(1), w1(1), wOut(1);
    ANDGate gate(&w0, &w1, &wOut);
    // Set input signals via SignalSource
    SignalSource src0(1), src1(1);
    src0.drive(LogicVector(0));
    src1.drive(LogicVector(1));
    // Connect sources to wires
    w0.addSource(&src0);
    w1.addSource(&src1);
    // Simulate propagation (notify chain)
    EXPECT_TRUE(w0.notify());
    EXPECT_TRUE(w1.notify());
    // Now the AND gate should recalculate and set output to 0 (0 AND 1)
    // Since internal recalculate is private, we verify via output wire state.
    // Assuming Wire::peek returns LogicVector.
    // The output wire should have logic 0 after propagation.
    // Trigger gate recalculation by notifying input wires.
    EXPECT_TRUE(gate["in0"]->notify());
    EXPECT_TRUE(gate["in1"]->notify());
    // Output wire should reflect AND result.
    // Note: This test assumes that Wire::peek returns LogicVector and that
    // logical 0 is represented by LogicVector(0).
    EXPECT_EQ(wOut.peek(), LogicVector(0));
}

// Additional tests could include varying all combinations of inputs.
// ---------------------------------------------------------------------------
// Additional ANDGate exhaustive tests
// ---------------------------------------------------------------------------

// Verify the truth table for a 1‑bit AND gate.
TEST(ANDGateTest, TruthTable)
{
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), out(bw);
    ANDGate gate(&in0, &in1, &out);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            src0.drive(LogicVector::FromBool(a));
            src1.drive(LogicVector::FromBool(b));
            // Force propagation by notifying the output wire.
            out.notify();
            bool expected = a && b;
            EXPECT_EQ((bool)out.peek(), expected);
        }
    }
}

// Ensure that a TTL that expires prevents the output from changing.
TEST(ANDGateTest, TtlStopsPropagation)
{
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), out(bw);
    ANDGate gate(&in0, &in1, &out);
    SignalSource src0(bw);
    Constant src1(LogicVector::FromBool(true), bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);

    // Drive first input with normal TTL, second with TTL = 0.
    src0.drive(LogicVector::FromBool(false));
    bool ok = src0.drive(LogicVector::FromBool(true), 0);
    EXPECT_FALSE(ok);

    // As ttl expired, the output should not reflect the AND of inputs.
    EXPECT_EQ(out.peek(), LogicVector::Zero());
}

// Chain two AND gates and verify the final result.
TEST(ANDGateTest, ChainedGates)
{
    const bitWidth_t bw = 8;
    Wire w0(bw), w1(bw), w2(bw), wMid(bw), wOut(bw);
    ANDGate g1(&w0, &w1, &wMid), g2(&wMid, &w2, &wOut);
    SignalSource s0(bw), s1(bw), s2(bw);

    s0.addTarget(&w0);
    s1.addTarget(&w1);
    s2.addTarget(&w2);

    // Drive inputs
    LogicVector a = LogicVector::FromInt(0xAA);
    LogicVector b = LogicVector::FromInt(0x0F);
    LogicVector c = LogicVector::FromInt(0xFF);
    s0.drive(a);
    s1.drive(b);
    s2.drive(c);

    // Propagation
    wOut.notify();
    LogicVector expected = (a & b) & c;
    EXPECT_EQ(wOut.peek(), expected);
}

// Create a feedback loop and verify that TTL eventually expires, preventing a stack overflow.
TEST(ANDGateTest, FeedbackLoopWithTtl)
{
    const bitWidth_t bw = 4;
    Wire wIn(bw), wOut(bw);
    ANDGate gate(&wIn, &wOut, &wOut);

    SignalSource src(bw);
    src.addTarget(&wIn);
    src.drive(LogicVector::FromInt(0xF), 5); // limited TTL
    // Propagation should terminate with TTL expiration.
    bool ttlExpired = wOut.notify();
    EXPECT_TRUE(ttlExpired);
}