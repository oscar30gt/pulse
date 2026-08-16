#include <gtest/gtest.h>
#include "../include/component.h"
#include "../include/wire.h"
#include "../include/gates.h"
#include "../include/signalDrain.h"
#include "../include/signalSource.h"

using namespace Pulse;

TEST(PropagationTest, SimpleWirePropagation) {
    const bitWidth_t bw = 8;
    SignalSource src(bw);
    SignalDrain drain(bw);
    Wire wire(bw);
    src.addTarget(&wire);
    drain.addSource(&wire);
    LogicVector val = LogicVector::FromInt(0xAA);
    EXPECT_TRUE(src.drive(val));
    EXPECT_EQ(drain.pull(), val);
}

TEST(PropagationTest, TTLExpiration) {
    const bitWidth_t bw = 4;
    SignalSource src(bw);
    Wire wire(bw);
    src.addTarget(&wire);
    LogicVector val = LogicVector::FromInt(0x5);
    // Use zero TTL to force immediate expiration
    EXPECT_FALSE(src.drive(val, 0));
}

TEST(PropagationTest, SelfChainedWire) {
    const bitWidth_t bw = 4;
    SignalSource src(bw);
    Wire wire(bw);
    src.addTarget(&wire); // Self-loop
    wire.addTarget(&wire);
    LogicVector val = LogicVector::FromInt(0x3333);
    // Should not cause infinite recursion.
    // ttl not even expires because the value doesnt change after the first propagation.
    EXPECT_TRUE(src.drive(val));
}

TEST(PropagationTest, SelfChainedAND) {
    const bitWidth_t bw = 4;
    Wire wIn(bw), wOut(bw);
    // Connect output back to one input to create a loop
    BinaryGate gate(&wIn, &wOut, &wOut, BinaryOperation::AND);
    // Drive input
    SignalSource src(bw);
    src.addTarget(&wIn);
    LogicVector val = LogicVector::FromInt(0xF);
    src.drive(val);
    // Should not cause infinite recursion; drive returns true
    EXPECT_TRUE(src.drive(val));
}

TEST(PropagationTest, AndGatePropagation) {
    const bitWidth_t bw = 8;
    SignalSource src0(bw);
    SignalSource src1(bw);
    Wire w0(bw), w1(bw), wOut(bw);
    src0.addTarget(&w0);
    src1.addTarget(&w1);
    BinaryGate andGate(&w0, &w1, &wOut, BinaryOperation::AND);
    SignalDrain drain(bw);
    drain.addSource(&wOut);
    LogicVector a = LogicVector::FromInt(0x0F);
    LogicVector b = LogicVector::FromInt(0xF0);
    src0.drive(a);
    src1.drive(b);
    LogicVector expected = a & b;
    EXPECT_EQ(drain.pull(), expected);
}

TEST(PropagationTest, LoopPrevention) {
    const bitWidth_t bw = 8;
    Wire wIn(bw), wOut(bw);
    // Connect output back to one input to create a loop
    BinaryGate gate(&wIn, &wOut, &wOut, BinaryOperation::AND);
    // Drive input
    SignalSource src(bw);
    src.addTarget(&wIn);
    LogicVector val = LogicVector::FromInt(0xFF);
    src.drive(val);
    // Should not cause infinite recursion; drive returns true
    EXPECT_TRUE(src.drive(val));
}
 
// Additional complex propagation tests

TEST(PropagationTest, ChainedAndGates) {
    const bitWidth_t bw = 8;
    SignalSource src0(bw), src1(bw);
    Wire w0(bw), w1(bw), wMid(bw), wOut(bw);
    src0.addTarget(&w0);
    src1.addTarget(&w1);
    // First gate connections
    BinaryGate firstGate(&w0, &w1, &wMid, BinaryOperation::AND);
    // Second gate connections, feeding from first gate output and another source
    SignalSource src2(bw);
    Wire w2(bw);
    src2.addTarget(&w2);
    BinaryGate secondGate(&wMid, &w2, &wOut, BinaryOperation::AND);
    SignalDrain drain(bw);
    drain.addSource(&wOut);
    // Drive inputs
    LogicVector a = LogicVector::FromInt(0xAA);
    LogicVector b = LogicVector::FromInt(0x55);
    LogicVector c = LogicVector::FromInt(0xFF);
    src0.drive(a);
    src1.drive(b);
    src2.drive(c);
    LogicVector expected = (a & b) & c;
    EXPECT_EQ(drain.pull(), expected);
}

TEST(PropagationTest, MultiTargetDrain) {
    const bitWidth_t bw = 4;
    SignalSource src(bw);
    Wire w(bw);
    src.addTarget(&w);
    SignalDrain drain1(bw), drain2(bw), drain3(bw);
    drain1.addSource(&w);
    drain2.addSource(&w);
    drain3.addSource(&w);
    LogicVector val = LogicVector::FromInt(0xA);
    src.drive(val);
    EXPECT_EQ(drain1.pull(), val);
    EXPECT_EQ(drain2.pull(), val);
    EXPECT_EQ(drain3.pull(), val);
}

TEST(PropagationTest, ComplexLoopWithTTL) {
    const bitWidth_t bw = 8;
    Wire wInA(bw), wInB(bw), wMid(bw), wOut(bw);
    // Connect gates to form a loop: A.out -> wMid -> B.in0, B.out -> wOut -> A.in1, and also feed back wOut -> A.in0
    BinaryGate gateA(&wInA, &wOut, &wMid, BinaryOperation::AND);
    BinaryGate gateB(&wMid, &wInB, &wOut, BinaryOperation::AND);
    // Create sources and drains
    SignalSource srcA(bw), srcB(bw);
    srcA.addTarget(&wInA);
    srcB.addTarget(&wInB);
    SignalDrain drain(bw);
    drain.addSource(&wOut);
    // Drive both inputs; TTL set to low value to prevent infinite recursion
    LogicVector valA = LogicVector::FromInt(0xFF);
    LogicVector valB = LogicVector::FromInt(0x0F);
    srcA.drive(valA);
    srcB.drive(valB);
    // Propagation should terminate with TTL expiration; drive with zero TTL fails
    EXPECT_FALSE(srcA.drive(valA, 0));
}

