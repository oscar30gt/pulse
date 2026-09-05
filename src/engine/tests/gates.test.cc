// gates_test.cpp
// Tests for Pulse::ANDGate using GoogleTest.

#include <gtest/gtest.h>
#include "gates.h"
#include "wire.h"
#include "signalDrain.h"
#include "signalSource.h"
#include "constant.h"

using namespace Pulse;
using namespace Pulse::Engine;
 
// ===========================================================================
// OR GATE TESTS
// ===========================================================================
 
TEST(ORGateTest, ConstructionCreatesPorts)
{
    Wire w0(1), w1(1), wOut(1);
    BinaryGate gate(&w0, &w1, &wOut, BinaryOp::OR);
    EXPECT_TRUE(gate.hasInputPort("in0"));
    EXPECT_TRUE(gate.hasInputPort("in1"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
    EXPECT_FALSE(gate.hasPort("nonexistent"));
}
 
TEST(ORGateTest, TruthTable)
{
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::OR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    // Truth table: 0|0=0, 0|1=1, 1|0=1, 1|1=1
    int truthTable[2][2] = { {0, 1}, {1, 1} };
 
    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            src0.drive(LogicVector::FromBool(a));
            src1.drive(LogicVector::FromBool(b));
            bool expected = truthTable[a][b];
            EXPECT_EQ((bool)out.peek(), expected);
        }
    }
}
 
TEST(ORGateTest, WideBitsTruthTable)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::OR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    LogicVector a = LogicVector::FromInt(0xF0);
    LogicVector b = LogicVector::FromInt(0x0F);
    src0.drive(a);
    src1.drive(b);
 
    LogicVector expected = a | b; // 0xFF
    EXPECT_EQ(out.peek(), expected);
}
 
 
 
TEST(ORGateTest, ChainedGates)
{
    const bitWidth_t bw = 8;
    Wire w0(bw), w1(bw), w2(bw), wMid(bw), wOut(bw);
    BinaryGate g1(&w0, &w1, &wMid, BinaryOp::OR);
    BinaryGate g2(&wMid, &w2, &wOut, BinaryOp::OR);
    SignalSource s0(bw), s1(bw), s2(bw);
 
    s0.addTarget(&w0);
    s1.addTarget(&w1);
    s2.addTarget(&w2);
 
    LogicVector a = LogicVector::FromInt(0x80);
    LogicVector b = LogicVector::FromInt(0x40);
    LogicVector c = LogicVector::FromInt(0x20);
    s0.drive(a);
    s1.drive(b);
    s2.drive(c);
 
    LogicVector expected = (a | b) | c; // 0xE0
    EXPECT_EQ(wOut.peek(), expected);
}
 
// ===========================================================================
// XOR GATE TESTS
// ===========================================================================
 
TEST(XORGateTest, ConstructionCreatesPorts)
{
    Wire w0(1), w1(1), wOut(1);
    BinaryGate gate(&w0, &w1, &wOut, BinaryOp::XOR);
    EXPECT_TRUE(gate.hasInputPort("in0"));
    EXPECT_TRUE(gate.hasInputPort("in1"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
}
 
TEST(XORGateTest, TruthTable)
{
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::XOR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    // Truth table: 0^0=0, 0^1=1, 1^0=1, 1^1=0
    int truthTable[2][2] = { {0, 1}, {1, 0} };
 
    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            src0.drive(LogicVector::FromBool(a));
            src1.drive(LogicVector::FromBool(b));
            bool expected = truthTable[a][b];
            EXPECT_EQ((bool)out.peek(), expected);
        }
    }
}
 
TEST(XORGateTest, WideBitsParityDetection)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::XOR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    LogicVector a = LogicVector::FromInt(0xAA); // 10101010
    LogicVector b = LogicVector::FromInt(0x55); // 01010101
    src0.drive(a);
    src1.drive(b);
 
    LogicVector expected = a ^ b; // 0xFF (all bits differ)
    EXPECT_EQ(out.peek(), expected);
}
 
TEST(XORGateTest, ChainedGates)
{
    const bitWidth_t bw = 8;
    Wire w0(bw), w1(bw), w2(bw), wMid(bw), wOut(bw);
    BinaryGate g1(&w0, &w1, &wMid, BinaryOp::XOR);
    BinaryGate g2(&wMid, &w2, &wOut, BinaryOp::XOR);
    SignalSource s0(bw), s1(bw), s2(bw);
 
    s0.addTarget(&w0);
    s1.addTarget(&w1);
    s2.addTarget(&w2);
 
    LogicVector a = LogicVector::FromInt(0xFF);
    LogicVector b = LogicVector::FromInt(0x0F);
    LogicVector c = LogicVector::FromInt(0x0F);
    s0.drive(a);
    s1.drive(b);
    s2.drive(c);
 
    LogicVector expected = (a ^ b) ^ c; // 0xF0
    EXPECT_EQ(wOut.peek(), expected);
}
 
// ===========================================================================
// NAND GATE TESTS
// ===========================================================================
 
TEST(NANDGateTest, ConstructionCreatesPorts)
{
    Wire w0(1), w1(1), wOut(1);
    BinaryGate gate(&w0, &w1, &wOut, BinaryOp::NAND);
    EXPECT_TRUE(gate.hasInputPort("in0"));
    EXPECT_TRUE(gate.hasInputPort("in1"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
}
 
TEST(NANDGateTest, TruthTable)
{
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::NAND);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    // Truth table: NAND is NOT AND
    // 0 NAND 0 = 1, 0 NAND 1 = 1, 1 NAND 0 = 1, 1 NAND 1 = 0
    int truthTable[2][2] = { {1, 1}, {1, 0} };
 
    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            src0.drive(LogicVector::FromBool(a));
            src1.drive(LogicVector::FromBool(b));
            bool expected = truthTable[a][b];
            EXPECT_EQ(out.peek().bit(0) == '1', expected);
        }
    }
}
 
TEST(NANDGateTest, WideBitsInversion)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::NAND);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    LogicVector a = LogicVector::FromInt(0xFF);
    LogicVector b = LogicVector::FromInt(0xFF);
    src0.drive(a);
    src1.drive(b);
 
    // NAND(0xFF, 0xFF) should be 0x00
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '0');
    }
}
 
TEST(NANDGateTest, ChainedNANDGates)
{
    // Two NAND gates chained: demonstrates logical completeness
    const bitWidth_t bw = 1;
    Wire w0(bw), w1(bw), w2(bw), wMid(bw), wOut(bw);
    BinaryGate g1(&w0, &w1, &wMid, BinaryOp::NAND);
    BinaryGate g2(&wMid, &w2, &wOut, BinaryOp::NAND);
    SignalSource s0(bw), s1(bw), s2(bw);
 
    s0.addTarget(&w0);
    s1.addTarget(&w1);
    s2.addTarget(&w2);
 
    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            for (int c = 0; c <= 1; ++c)
            {
                s0.drive(LogicVector::FromBool(a));
                s1.drive(LogicVector::FromBool(b));
                s2.drive(LogicVector::FromBool(c));
 
                // First NAND: ~(a & b)
                // Second NAND: ~(~(a & b) & c)
                bool mid = !(a && b);
                bool expected = !(mid && c);
                char expectedChar = expected ? '1' : '0';
                EXPECT_EQ(wOut.peek().bit(0), expectedChar);
            }
        }
    }
}
 
// ===========================================================================
// NOR GATE TESTS
// ===========================================================================
 
TEST(NORGateTest, ConstructionCreatesPorts)
{
    Wire w0(1), w1(1), wOut(1);
    BinaryGate gate(&w0, &w1, &wOut, BinaryOp::NOR);
    EXPECT_TRUE(gate.hasInputPort("in0"));
    EXPECT_TRUE(gate.hasInputPort("in1"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
}
 
TEST(NORGateTest, TruthTable)
{
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::NOR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    // Truth table: NOR is NOT OR
    // 0 NOR 0 = 1, 0 NOR 1 = 0, 1 NOR 0 = 0, 1 NOR 1 = 0
    char truthTable[2][2] = { {'1', '0'}, {'0', '0'} };
 
    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            src0.drive(LogicVector::FromBool(a));
            src1.drive(LogicVector::FromBool(b));
            char expected = truthTable[a][b];
            EXPECT_EQ(out.peek().bit(0), expected);
        }
    }
}
 
TEST(NORGateTest, WideBits)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::NOR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    LogicVector a = LogicVector::FromInt(0xF0);
    LogicVector b = LogicVector::FromInt(0x0F);
    src0.drive(a);
    src1.drive(b);
 
    // NOR(0xF0, 0x0F) = NOT(0xFF) = 0x00
    // Check each bit is 0
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '0');
    }
}
 
// ===========================================================================
// XNOR GATE TESTS
// ===========================================================================
 
TEST(XNORGateTest, ConstructionCreatesPorts)
{
    Wire w0(1), w1(1), wOut(1);
    BinaryGate gate(&w0, &w1, &wOut, BinaryOp::XNOR);
    EXPECT_TRUE(gate.hasInputPort("in0"));
    EXPECT_TRUE(gate.hasInputPort("in1"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
}
 
TEST(XNORGateTest, TruthTable)
{
    const bitWidth_t bw = 1;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::XNOR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    // Truth table: XNOR is NOT XOR (equivalence)
    // 0 XNOR 0 = 1, 0 XNOR 1 = 0, 1 XNOR 0 = 0, 1 XNOR 1 = 1
    char truthTable[2][2] = { {'1', '0'}, {'0', '1'} };
 
    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            src0.drive(LogicVector::FromBool(a));
            src1.drive(LogicVector::FromBool(b));
            char expected = truthTable[a][b];
            EXPECT_EQ(out.peek().bit(0), expected);
        }
    }
}
 
TEST(XNORGateTest, EquivalenceDetector)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::XNOR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    // When inputs are the same, XNOR output is all 1s
    LogicVector same = LogicVector::FromInt(0x5A);
    src0.drive(same);
    src1.drive(same);
 
    // Check each bit is 1
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '1');
    }
}
 
TEST(XNORGateTest, WideBitsInversion)
{
    const bitWidth_t bw = 8;
    Wire in0(bw), in1(bw), out(bw);
    BinaryGate gate(&in0, &in1, &out, BinaryOp::XNOR);
    SignalSource src0(bw), src1(bw);
    src0.addTarget(&in0);
    src1.addTarget(&in1);
 
    LogicVector a = LogicVector::FromInt(0xAA);
    LogicVector b = LogicVector::FromInt(0x55);
    src0.drive(a);
    src1.drive(b);
 
    // XNOR of complementary patterns should be all 0s
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '0');
    }
}
 
// ===========================================================================
// NOT GATE TESTS
// ===========================================================================
 
TEST(NOTGateTest, ConstructionCreatesPorts)
{
    Wire wIn(1), wOut(1);
    NOTGate gate(&wIn, &wOut);
    EXPECT_TRUE(gate.hasInputPort("in"));
    EXPECT_TRUE(gate.hasOutputPort("out"));
    EXPECT_FALSE(gate.hasPort("nonexistent"));
}
 
TEST(NOTGateTest, TruthTable)
{
    const bitWidth_t bw = 1;
    Wire in(bw), out(bw);
    NOTGate gate(&in, &out);
    SignalSource src(bw);
    src.addTarget(&in);
 
    // Truth table: NOT 0 = 1, NOT 1 = 0
    char truthTable[2] = {'1', '0'};
    
    for (int a = 0; a <= 1; ++a)
    {
        src.drive(LogicVector::FromBool(a));
        char expected = truthTable[a];
        EXPECT_EQ(out.peek().bit(0), expected);
    }
}
 
TEST(NOTGateTest, WideBitsInversion)
{
    const bitWidth_t bw = 8;
    Wire in(bw), out(bw);
    NOTGate gate(&in, &out);
    SignalSource src(bw);
    src.addTarget(&in);
 
    LogicVector a = LogicVector::FromInt(0xF0); // 11110000
    src.drive(a);
 
    // Check bits 0-3 are 1 (inverted from 0s)
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '1');
    }
    // Check bits 4-7 are 0 (inverted from 1s)
    for (int i = 4; i < 8; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '0');
    }
}
 
TEST(NOTGateTest, DoubleNegation)
{
    const bitWidth_t bw = 8;
    Wire wIn(bw), wMid(bw), wOut(bw);
    NOTGate g1(&wIn, &wMid);
    NOTGate g2(&wMid, &wOut);
    SignalSource src(bw);
    src.addTarget(&wIn);
 
    LogicVector a = LogicVector::FromInt(0xAA); // 10101010
    src.drive(a);
 
    // NOT(NOT(a)) == a
    EXPECT_EQ(wOut.peek(), a);
}
 
TEST(NOTGateTest, AllOnes)
{
    const bitWidth_t bw = 8;
    Wire in(bw), out(bw);
    NOTGate gate(&in, &out);
    SignalSource src(bw);
    src.addTarget(&in);
 
    LogicVector all_ones = LogicVector::FromInt(0xFF);
    src.drive(all_ones);
 
    // All bits should be 0 after inversion
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '0');
    }
}
 
TEST(NOTGateTest, AllZeros)
{
    const bitWidth_t bw = 8;
    Wire in(bw), out(bw);
    NOTGate gate(&in, &out);
    SignalSource src(bw);
    src.addTarget(&in);
 
    LogicVector all_zeros = LogicVector::Zero();
    src.drive(all_zeros);
 
    // All bits should be 1 after inversion
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(out.peek().bit(i), '1');
    }
}
 
// ===========================================================================
// MIXED GATE COMBINATIONS
// ===========================================================================
 
TEST(MixedGatesTest, DeMoregansLaw_NOT_AND_eq_OR_NOT)
{
    // De Morgan's Law: NOT(A AND B) = NOT(A) OR NOT(B)
    const bitWidth_t bw = 1;
    
    // Left side: NOT(A AND B)
    Wire wA1(bw), wB1(bw), wAndOut(bw), wNotOut1(bw);
    BinaryGate andGate(&wA1, &wB1, &wAndOut, BinaryOp::AND);
    NOTGate notGate1(&wAndOut, &wNotOut1);
    
    // Right side: NOT(A) OR NOT(B)
    Wire wA2(bw), wB2(bw), wNotA(bw), wNotB(bw), wOrOut(bw);
    NOTGate notGateA(&wA2, &wNotA);
    NOTGate notGateB(&wB2, &wNotB);
    BinaryGate orGate(&wNotA, &wNotB, &wOrOut, BinaryOp::OR);
    
    SignalSource srcA1(bw), srcB1(bw), srcA2(bw), srcB2(bw);
    srcA1.addTarget(&wA1);
    srcB1.addTarget(&wB1);
    srcA2.addTarget(&wA2);
    srcB2.addTarget(&wB2);
    
    for (int a = 0; a <= 1; ++a)
    {
        for (int b = 0; b <= 1; ++b)
        {
            srcA1.drive(LogicVector::FromBool(a));
            srcB1.drive(LogicVector::FromBool(b));
            srcA2.drive(LogicVector::FromBool(a));
            srcB2.drive(LogicVector::FromBool(b));
            
            EXPECT_EQ(wNotOut1.peek(), wOrOut.peek());
        }
    }
}
 
TEST(MixedGatesTest, Multiplexer2to1)
{
    // Simple 2-to-1 multiplexer using AND, OR, and NOT gates
    // Output = (sel ? a : b) = (a AND sel) OR (b AND NOT(sel))
    const bitWidth_t bw = 1;
    
    Wire wA(bw), wB(bw), wSel(bw);
    Wire wNotSel(bw), wAndA(bw), wAndB(bw), wOut(bw);
    
    NOTGate notGate(&wSel, &wNotSel);
    BinaryGate andGateA(&wA, &wSel, &wAndA, BinaryOp::AND);
    BinaryGate andGateB(&wB, &wNotSel, &wAndB, BinaryOp::AND);
    BinaryGate orGate(&wAndA, &wAndB, &wOut, BinaryOp::OR);
    
    SignalSource srcA(bw), srcB(bw), srcSel(bw);
    srcA.addTarget(&wA);
    srcB.addTarget(&wB);
    srcSel.addTarget(&wSel);
    
    // Test case: sel=0 -> output should be B
    srcA.drive(LogicVector::FromBool(true));
    srcB.drive(LogicVector::FromBool(false));
    srcSel.drive(LogicVector::FromBool(false));
    EXPECT_EQ((bool)wOut.peek(), false); // B
    
    // Test case: sel=1 -> output should be A
    srcA.drive(LogicVector::FromBool(true));
    srcB.drive(LogicVector::FromBool(false));
    srcSel.drive(LogicVector::FromBool(true));
    EXPECT_EQ((bool)wOut.peek(), true); // A
}
 
TEST(MixedGatesTest, FullAdderCarout)
{
    // Carry out for full adder: Cout = (A AND B) OR (Cin AND (A XOR B))
    const bitWidth_t bw = 1;
    
    Wire wA(bw), wB(bw), wCin(bw);
    Wire wAndAB(bw), wXorAB(bw), wAndCinXor(bw), wCout(bw);
    
    BinaryGate andAB(&wA, &wB, &wAndAB, BinaryOp::AND);
    BinaryGate xorAB(&wA, &wB, &wXorAB, BinaryOp::XOR);
    BinaryGate andCinXor(&wCin, &wXorAB, &wAndCinXor, BinaryOp::AND);
    BinaryGate orOut(&wAndAB, &wAndCinXor, &wCout, BinaryOp::OR);
    
    SignalSource srcA(bw), srcB(bw), srcCin(bw);
    srcA.addTarget(&wA);
    srcB.addTarget(&wB);
    srcCin.addTarget(&wCin);
    
    // Test: A=1, B=1, Cin=0 -> Cout=1
    srcA.drive(LogicVector::FromBool(1));
    srcB.drive(LogicVector::FromBool(1));
    srcCin.drive(LogicVector::FromBool(0));
    EXPECT_EQ((bool)wCout.peek(), true);
    
    // Test: A=1, B=0, Cin=1 -> Cout=1
    srcA.drive(LogicVector::FromBool(1));
    srcB.drive(LogicVector::FromBool(0));
    srcCin.drive(LogicVector::FromBool(1));
    EXPECT_EQ((bool)wCout.peek(), true);
    
    // Test: A=0, B=0, Cin=0 -> Cout=0
    srcA.drive(LogicVector::FromBool(0));
    srcB.drive(LogicVector::FromBool(0));
    srcCin.drive(LogicVector::FromBool(0));
    EXPECT_EQ((bool)wCout.peek(), false);
}
 
TEST(MixedGatesTest, SRFlipFlop) {
    Wire set(1), reset(1), q(1), qNot(1);

    SignalSource srcSet(1), srcReset(1);
    srcSet.addTarget(&set);
    srcReset.addTarget(&reset);

    // SR Flip-Flop using NOR
    BinaryGate nor1(&reset, &qNot, &q, BinaryOp::NOR); // q = reset NOR qNot
    BinaryGate nor2(&set, &q, &qNot, BinaryOp::NOR); // qNot = set NOR q
    
    srcSet.drive(LogicVector::FromBool(0));
    srcReset.drive(LogicVector::FromBool(0));

    srcSet.drive(LogicVector::FromBool(1)); // Set
    EXPECT_EQ((bool)q.peek(), true);
    EXPECT_EQ((bool)qNot.peek(), false);
    srcSet.drive(LogicVector::FromBool(0));
    
    srcReset.drive(LogicVector::FromBool(1)); // Reset
    EXPECT_EQ((bool)q.peek(), false);
    EXPECT_EQ((bool)qNot.peek(), true);
    srcReset.drive(LogicVector::FromBool(0));
    
    srcSet.drive(LogicVector::FromBool(1)); // Set again
    EXPECT_EQ((bool)q.peek(), true);
    EXPECT_EQ((bool)qNot.peek(), false);
}