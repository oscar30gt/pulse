#include <gtest/gtest.h>
#include <cstdint>

#include "logicVector.h"

using namespace Pulse;

// ============================================================================
// Helper function: Create a single-bit LogicVector with a specific state
// ============================================================================

static LogicVector make1Bit(char state)
{
    LogicVector v{ 0ULL, 0ULL };
    v.setBit(0, state);
    return v;
}

// ============================================================================
// 1. Factory Methods & Default Initialization
// ============================================================================

TEST(LogicVectorTest, DefaultInitializationIsHighZ)
{
    LogicVector vec;
    EXPECT_EQ(vec.value, ~0ULL);
    EXPECT_EQ(vec.mask, ~0ULL);
    EXPECT_EQ(vec, LogicVector::HighZ());
}

TEST(LogicVectorTest, FactoryZero)
{
    LogicVector vec = LogicVector::Zero();
    EXPECT_EQ(vec.value, 0ULL);
    EXPECT_EQ(vec.mask, 0ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.getBit(i), '0');
    }
}

TEST(LogicVectorTest, FactoryOnes)
{
    LogicVector vec = LogicVector::Ones();
    EXPECT_EQ(vec.value, ~0ULL);
    EXPECT_EQ(vec.mask, 0ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.getBit(i), '1');
    }
}

TEST(LogicVectorTest, FactoryUnknown)
{
    LogicVector vec = LogicVector::Unknown();
    EXPECT_EQ(vec.value, 0ULL);
    EXPECT_EQ(vec.mask, ~0ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.getBit(i), 'X');
    }
}

TEST(LogicVectorTest, FactoryHighZ)
{
    LogicVector vec = LogicVector::HighZ();
    EXPECT_EQ(vec.value, ~0ULL);
    EXPECT_EQ(vec.mask, ~0ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.getBit(i), 'Z');
    }
}

TEST(LogicVectorTest, FactoryFromInt)
{
    LogicVector v1 = LogicVector::FromInt(0ULL);
    EXPECT_EQ(v1, LogicVector::Zero());

    LogicVector v2 = LogicVector::FromInt(~0ULL);
    EXPECT_EQ(v2, LogicVector::Ones());

    LogicVector v3 = LogicVector::FromInt(0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(v3.value, 0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(v3.mask, 0ULL);

    LogicVector v4 = LogicVector::FromInt(0x5555555555555555ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(v4.getBit(i), (i % 2 == 0) ? '1' : '0');
    }
}

TEST(LogicVectorTest, FactoryFromBool)
{
    LogicVector vTrue = LogicVector::FromBool(true);
    EXPECT_EQ(vTrue.value, 1ULL);
    EXPECT_EQ(vTrue.mask, 0ULL);
    EXPECT_EQ(vTrue.getBit(0), '1');
    EXPECT_EQ(vTrue.getBit(1), '0');

    LogicVector vFalse = LogicVector::FromBool(false);
    EXPECT_EQ(vFalse, LogicVector::Zero());
}

// ============================================================================
// 2. Boolean Conversion & Equality
// ============================================================================

TEST(LogicVectorTest, ExplicitBoolConversion)
{
    EXPECT_FALSE(static_cast<bool>(LogicVector::Zero()));
    EXPECT_TRUE(static_cast<bool>(LogicVector::Ones()));
    EXPECT_TRUE(static_cast<bool>(LogicVector::Unknown()));
    EXPECT_TRUE(static_cast<bool>(LogicVector::HighZ()));
    EXPECT_TRUE(static_cast<bool>(LogicVector::FromInt(1)));
    EXPECT_TRUE(static_cast<bool>(LogicVector::FromInt(1ULL << 63)));

    LogicVector customZero{ 0ULL, 0ULL };
    EXPECT_FALSE(static_cast<bool>(customZero));

    LogicVector onlyMask{ 0ULL, 1ULL };
    EXPECT_TRUE(static_cast<bool>(onlyMask));
}

TEST(LogicVectorTest, ExplicitIntegerConversion)
{
    LogicVector vec = LogicVector::FromInt(0x123456789ABCDEF0ULL);

    EXPECT_EQ(static_cast<uint64_t>(vec), 0x123456789ABCDEF0ULL);
    EXPECT_EQ(static_cast<uint32_t>(vec), 0x9ABCDEF0ULL);
    EXPECT_EQ(static_cast<uint16_t>(vec), 0xDEF0ULL);
    EXPECT_EQ(static_cast<uint8_t>(vec), 0xF0ULL);
}

TEST(LogicVectorTest, EqualityAndInequality)
{
    LogicVector a = LogicVector::FromInt(0x12345678ULL);
    LogicVector b = LogicVector::FromInt(0x12345678ULL);
    LogicVector c = LogicVector::FromInt(0x87654321ULL);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);

    LogicVector x = LogicVector::Unknown();
    LogicVector z = LogicVector::HighZ();
    EXPECT_FALSE(x == z);
    EXPECT_TRUE(x != z);

    // Difference in mask only
    LogicVector m1{ 0x10ULL, 0x01ULL };
    LogicVector m2{ 0x10ULL, 0x02ULL };
    EXPECT_NE(m1, m2);

    // Difference in value only
    LogicVector v1{ 0x10ULL, 0x01ULL };
    LogicVector v2{ 0x20ULL, 0x01ULL };
    EXPECT_NE(v1, v2);
}

// ============================================================================
// 3. Single Bit Access via getBit/setBit
// ============================================================================

TEST(LogicVectorTest, GetBitRead)
{
    LogicVector vec = LogicVector::FromInt(0xAAAAAAAAAAAAAAAAULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        char expected = (i % 2 == 0) ? '0' : '1';
        EXPECT_EQ(vec.getBit(i), expected);
    }
}

TEST(LogicVectorTest, SetBitWrite)
{
    LogicVector vec = LogicVector::Zero();

    vec.setBit(0, '1');
    vec.setBit(5, 'X');
    vec.setBit(10, 'Z');
    vec.setBit(63, '1');

    EXPECT_EQ(vec.getBit(0), '1');
    EXPECT_EQ(vec.getBit(1), '0');
    EXPECT_EQ(vec.getBit(5), 'X');
    EXPECT_EQ(vec.getBit(10), 'Z');
    EXPECT_EQ(vec.getBit(63), '1');

    // Overwrite existing states
    vec.setBit(0, '0');
    EXPECT_EQ(vec.getBit(0), '0');
    vec.setBit(5, '1');
    EXPECT_EQ(vec.getBit(5), '1');
    vec.setBit(10, 'X');
    EXPECT_EQ(vec.getBit(10), 'X');
}

TEST(LogicVectorTest, SetBitAllStates)
{
    LogicVector vec = LogicVector::Zero();

    // Set to each state and verify
    vec.setBit(0, '0');
    EXPECT_EQ(vec.getBit(0), '0');

    vec.setBit(0, '1');
    EXPECT_EQ(vec.getBit(0), '1');

    vec.setBit(0, 'X');
    EXPECT_EQ(vec.getBit(0), 'X');

    vec.setBit(0, 'Z');
    EXPECT_EQ(vec.getBit(0), 'Z');

    // Invalid character (ignored)
    vec.setBit(0, '?');
    EXPECT_EQ(vec.getBit(0), 'Z'); // Previous value unchanged
}

TEST(LogicVectorTest, GetBitOutOfBounds)
{
    LogicVector vec = LogicVector::Zero();
    EXPECT_THROW(vec.getBit(64), std::out_of_range);
    EXPECT_THROW(vec.getBit(100), std::out_of_range);
}

TEST(LogicVectorTest, SetBitOutOfBounds)
{
    LogicVector vec = LogicVector::Zero();
    EXPECT_THROW(vec.setBit(64, '1'), std::out_of_range);
    EXPECT_THROW(vec.setBit(100, 'X'), std::out_of_range);
}

TEST(LogicVectorTest, SequentialBitModification)
{
    LogicVector vec = LogicVector::Zero();

    // Set bits sequentially with different states
    for (uint8_t i = 0; i < 16; ++i)
    {
        char states[] = { '0', '1', 'X', 'Z' };
        vec.setBit(i, states[i % 4]);
    }

    // Verify all bits
    for (uint8_t i = 0; i < 16; ++i)
    {
        char expected = "01XZ"[i % 4];
        EXPECT_EQ(vec.getBit(i), expected);
    }
}

// ============================================================================
// 4. Range Access via getRange/setRange (Standard order: high >= low)
// ============================================================================

TEST(LogicVectorTest, GetRangeStandardOrder)
{
    LogicVector vec = LogicVector::FromInt(0xDEADBEEF01234567ULL);

    // Get lower byte (bits 7 down to 0): 0x67
    LogicVector byte0 = vec.getRange(7, 0);
    EXPECT_EQ(byte0, LogicVector::FromInt(0x67));

    // Get second byte (bits 15 down to 8): 0x45
    LogicVector byte1 = vec.getRange(15, 8);
    EXPECT_EQ(byte1, LogicVector::FromInt(0x45));

    // Get upper 32 bits (bits 63 down to 32): 0xDEADBEEF
    LogicVector upper32 = vec.getRange(63, 32);
    EXPECT_EQ(upper32, LogicVector::FromInt(0xDEADBEEFULL));

    // Single bit via range (bit 0)
    EXPECT_EQ(vec.getRange(0, 0), LogicVector::FromInt(1));
    EXPECT_EQ(vec.getRange(1, 1), LogicVector::FromInt(1));
    EXPECT_EQ(vec.getRange(2, 2), LogicVector::FromInt(1));
    EXPECT_EQ(vec.getRange(3, 3), LogicVector::FromInt(0));
}

TEST(LogicVectorTest, GetRangeReversal)
{
    // Test VHDL downto-style range extraction with reversal
    // 0010 = bits[3:0] = 0,0,1,0
    LogicVector vec = LogicVector::FromInt(0b0010); 

    // getRange(1, 0): normal order, extract bits 1,0 = 1,0 -> result 10 (binary) = 2
    LogicVector r1 = vec.getRange(1, 0);
    EXPECT_EQ(r1, LogicVector::FromInt(0b10));

    // getRange(0, 1): reversed order, extract bits 0,1 with reversal = 0,1 reversed -> 1,0 = 10 -> 2
    LogicVector r2 = vec.getRange(0, 1);
    EXPECT_EQ(r2, LogicVector::FromInt(0b01));

    // Test with asymmetric pattern: 1011 (bits 3,2,1,0 = 1,0,1,1)
    LogicVector vec2 = LogicVector::FromInt(0x0B);
    
    // getRange(3, 1): normal, bits 3,2,1 = 1,0,1 -> 101 = 5
    LogicVector r3 = vec2.getRange(3, 1);
    EXPECT_EQ(r3, LogicVector::FromInt(0x05));

    // getRange(1, 3): reversed, bits 1,2,3 with reversal = 1,0,1 reversed -> 1,0,1 = 101 = 5
    LogicVector r4 = vec2.getRange(1, 3);
    EXPECT_EQ(r4, LogicVector::FromInt(0x05));
}

TEST(LogicVectorTest, GetRangeWithMixedStates)
{
    LogicVector vec = LogicVector::HighZ(); // All bits Z
    vec.setBit(10, '0');
    vec.setBit(11, '1');
    vec.setBit(12, 'X');
    vec.setBit(13, 'Z');

    LogicVector sub = vec.getRange(13, 10);
    EXPECT_EQ(sub.getBit(0), '0');
    EXPECT_EQ(sub.getBit(1), '1');
    EXPECT_EQ(sub.getBit(2), 'X');
    EXPECT_EQ(sub.getBit(3), 'Z');
}

TEST(LogicVectorTest, SetRangeStandardOrder)
{
    LogicVector vec = LogicVector::Zero();

    // Write to lower byte (bits 7 down to 0)
    vec.setRange(7, 0, LogicVector::FromInt(0xFF));
    EXPECT_EQ(vec.getBit(0), '1');
    EXPECT_EQ(vec.getBit(7), '1');
    EXPECT_EQ(vec.getBit(8), '0');

    // Write to upper 32 bits (bits 63 down to 32)
    vec.setRange(63, 32, LogicVector::FromInt(0x12345678ULL));
    EXPECT_EQ(vec.getRange(63, 32), LogicVector::FromInt(0x12345678ULL));

    // Mixed states
    LogicVector mixed = LogicVector::Zero();
    mixed.setBit(0, 'X');
    mixed.setBit(1, 'Z');
    vec.setRange(15, 8, mixed);
    EXPECT_EQ(vec.getBit(8), 'X');
    EXPECT_EQ(vec.getBit(9), 'Z');
}

TEST(LogicVectorTest, SetRangeReversal)
{
    // Test example from user specification
    // 010101.setRange(5,2, 001100) ->110001
    LogicVector vec = LogicVector::FromInt(0x15);  // 010101
    LogicVector value = LogicVector::FromInt(0x0C); // 001100

    vec.setRange(5, 2, value);
    EXPECT_EQ(vec, LogicVector::FromInt(0x31)); // 110001

    // Test reversed example
    // 010101.setRange(2,5, 001100) ->001101
    LogicVector vec2 = LogicVector::FromInt(0x15);  // 010101
    vec2.setRange(2, 5, value);
    EXPECT_EQ(vec2, LogicVector::FromInt(0x0D)); // 001101
}

TEST(LogicVectorTest, SetRangePreservesOtherBits)
{
    LogicVector vec = LogicVector::FromInt(0xFFFFFFFFFFFFFFFFULL);

    // Write zeros to middle bits only
    vec.setRange(31, 16, LogicVector::Zero());

    // Verify lower bits unchanged
    EXPECT_EQ(vec.getRange(15, 0), LogicVector::FromInt(0xFFFFULL));

    // Verify upper bits unchanged
    EXPECT_EQ(vec.getRange(63, 32), LogicVector::FromInt(0xFFFFFFFFULL));

    // Verify middle bits are zero
    EXPECT_EQ(vec.getRange(31, 16), LogicVector::Zero());
}

TEST(LogicVectorTest, GetRangeOutOfBounds)
{
    LogicVector vec = LogicVector::FromInt(0x1234);

    EXPECT_THROW(vec.getRange(64, 0), std::out_of_range);   // start out of bounds
    EXPECT_THROW(vec.getRange(0, 64), std::out_of_range);   // end out of bounds
    EXPECT_THROW(vec.getRange(100, 90), std::out_of_range); // both out of bounds
}

TEST(LogicVectorTest, SetRangeOutOfBounds)
{
    LogicVector vec = LogicVector::FromInt(0x1234);

    EXPECT_THROW(vec.setRange(64, 0, LogicVector::Zero()), std::out_of_range);
    EXPECT_THROW(vec.setRange(0, 64, LogicVector::Zero()), std::out_of_range);
    EXPECT_THROW(vec.setRange(100, 90, LogicVector::Zero()), std::out_of_range);
}

TEST(LogicVectorTest, SetRangeFullVector)
{
    LogicVector vec = LogicVector::Zero();
    vec.setRange(63, 0, LogicVector::Ones());
    EXPECT_EQ(vec, LogicVector::Ones());
}

TEST(LogicVectorTest, SetRangeWithNarrowWidth)
{
    LogicVector vec = LogicVector::Zero();
    LogicVector pattern = LogicVector::FromInt(0xAA);

    vec.setRange(7, 0, pattern);
    EXPECT_EQ(vec.getRange(7, 0), pattern);

    vec.setRange(15, 8, pattern);
    EXPECT_EQ(vec.getRange(15, 8), pattern);
}

// ============================================================================
// 5. 4-State Bitwise Operations (AND, OR, XOR, NOT)
// ============================================================================

TEST(LogicVectorTest, BitwiseAndTruthTable)
{
    const char states[] = { '0', '1', 'X', 'Z' };
    const char expectedAnd[4][4] = {
        { '0', '0', '0', '0' },
        { '0', '1', 'X', 'X' },
        { '0', 'X', 'X', 'X' },
        { '0', 'X', 'X', 'X' }
    };

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            LogicVector a = make1Bit(states[i]);
            LogicVector b = make1Bit(states[j]);
            LogicVector res = a & b;
            EXPECT_EQ(res.getBit(0), expectedAnd[i][j])
                << "Failed AND for " << states[i] << " & " << states[j];
        }
    }
}

TEST(LogicVectorTest, BitwiseOrTruthTable)
{
    const char states[] = { '0', '1', 'X', 'Z' };
    const char expectedOr[4][4] = {
        { '0', '1', 'X', 'X' },
        { '1', '1', '1', '1' },
        { 'X', '1', 'X', 'X' },
        { 'X', '1', 'X', 'X' }
    };

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            LogicVector a = make1Bit(states[i]);
            LogicVector b = make1Bit(states[j]);
            LogicVector res = a | b;
            EXPECT_EQ(res.getBit(0), expectedOr[i][j])
                << "Failed OR for " << states[i] << " | " << states[j];
        }
    }
}

TEST(LogicVectorTest, BitwiseXorTruthTable)
{
    const char states[] = { '0', '1', 'X', 'Z' };
    const char expectedXor[4][4] = {
        { '0', '1', 'X', 'X' },
        { '1', '0', 'X', 'X' },
        { 'X', 'X', 'X', 'X' },
        { 'X', 'X', 'X', 'X' }
    };

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            LogicVector a = make1Bit(states[i]);
            LogicVector b = make1Bit(states[j]);
            LogicVector res = a ^ b;
            EXPECT_EQ(res.getBit(0), expectedXor[i][j])
                << "Failed XOR for " << states[i] << " ^ " << states[j];
        }
    }
}

TEST(LogicVectorTest, BitwiseNotTruthTable)
{
    EXPECT_EQ((~make1Bit('0')).getBit(0), '1');
    EXPECT_EQ((~make1Bit('1')).getBit(0), '0');
    EXPECT_EQ((~make1Bit('X')).getBit(0), 'X');
    EXPECT_EQ((~make1Bit('Z')).getBit(0), 'X');

    EXPECT_EQ(~LogicVector::Zero(), LogicVector::Ones());
    EXPECT_EQ(~LogicVector::Ones(), LogicVector::Zero());

    LogicVector val = LogicVector::FromInt(0x0123456789ABCDEFULL);
    EXPECT_EQ(~(~val), val);
}

TEST(LogicVectorTest, VectorWideBitwiseOperations)
{
    LogicVector a = LogicVector::FromInt(0x0F0F0F0F0F0F0F0FULL);
    LogicVector b = LogicVector::FromInt(0xF0F0F0F0F0F0F0F0ULL);

    EXPECT_EQ(a & b, LogicVector::Zero());
    EXPECT_EQ(a | b, LogicVector::Ones());
    EXPECT_EQ(a ^ b, LogicVector::Ones());

    LogicVector c = LogicVector::FromInt(0x3333333333333333ULL);
    EXPECT_EQ((a & b) & c, a & (b & c));
    EXPECT_EQ((a | b) | c, a | (b | c));
    EXPECT_EQ((a ^ b) ^ c, a ^ (b ^ c));
}

// ============================================================================
// 6. Addition (Commutative XOR for definite bits)
// ============================================================================

TEST(LogicVectorTest, AdditionDefiniteBits)
{
    LogicVector a = LogicVector::FromInt(0x0ULL);
    LogicVector b = LogicVector::FromInt(0x1ULL);

    EXPECT_EQ(a + a, LogicVector::FromInt(0x0ULL));
    EXPECT_EQ(a + b, LogicVector::FromInt(0x1ULL));
    EXPECT_EQ(b + b, LogicVector::FromInt(0x0ULL));

    EXPECT_EQ(a + b, b + a);
}

TEST(LogicVectorTest, AdditionWithUndefinedBits)
{
    LogicVector def = LogicVector::FromInt(0xFFULL);
    LogicVector unk = LogicVector::Unknown();
    LogicVector hz = LogicVector::HighZ();

    EXPECT_EQ(def + unk, LogicVector::Unknown());
    EXPECT_EQ(def + hz, LogicVector::Unknown());
    EXPECT_EQ(unk + hz, LogicVector::Unknown());
}

// ============================================================================
// 7. Logical Shifts (lsl, lsr)
// ============================================================================

TEST(LogicVectorTest, LogicalShiftLeft)
{
    LogicVector v = LogicVector::FromInt(0x000000000000000FULL);

    EXPECT_EQ(v.lsl(0), v);
    EXPECT_EQ(v.lsl(4), LogicVector::FromInt(0x00000000000000F0ULL));
    EXPECT_EQ(v.lsl(60), LogicVector::FromInt(0xF000000000000000ULL));
    EXPECT_EQ(v.lsl(64), LogicVector::Zero());
    EXPECT_EQ(v.lsl(70), LogicVector::Zero());

    // LSL expects a natural number. -2 is understood as a large positive shift, resulting in zero.
    EXPECT_EQ(v.lsl(-2), LogicVector::FromInt(0));

    LogicVector v8 = LogicVector::FromInt(0x0F);
    EXPECT_EQ(v8.lsl(2, 8), LogicVector::FromInt(0x3C));
    EXPECT_EQ(v8.lsl(4, 8), LogicVector::FromInt(0xF0));
    EXPECT_EQ(v8.lsl(8, 8), LogicVector::Zero());
}

TEST(LogicVectorTest, LogicalShiftRight)
{
    LogicVector v = LogicVector::FromInt(0xF000000000000000ULL);

    EXPECT_EQ(v.lsr(0), v);
    EXPECT_EQ(v.lsr(4), LogicVector::FromInt(0x0F00000000000000ULL));
    EXPECT_EQ(v.lsr(60), LogicVector::FromInt(0x000000000000000FULL));
    EXPECT_EQ(v.lsr(64), LogicVector::Zero());
    EXPECT_EQ(v.lsr(70), LogicVector::Zero());

    // LSR expects a natural number. -2 is understood as a large positive shift, resulting in zero.
    LogicVector small = LogicVector::FromInt(0x01);
    EXPECT_EQ(small.lsr(-4), LogicVector::FromInt(0));

    LogicVector v8 = LogicVector::FromInt(0xF0);
    EXPECT_EQ(v8.lsr(2, 8), LogicVector::FromInt(0x3C));
}

TEST(LogicVectorTest, ShiftWithMixedStates)
{
    LogicVector vec = LogicVector::Zero();
    vec.setBit(0, 'X');
    vec.setBit(1, 'Z');

    LogicVector shifted = vec.lsl(2);
    EXPECT_EQ(shifted.getBit(2), 'X');
    EXPECT_EQ(shifted.getBit(3), 'Z');
    EXPECT_EQ(shifted.getBit(0), '0');
}

// ============================================================================
// 8. Arithmetic Shift LogicalRight (asr)
// ============================================================================

TEST(LogicVectorTest, ArithmeticShiftRight)
{
    LogicVector pos = LogicVector::FromInt(0x4000000000000000ULL);
    EXPECT_EQ(pos.asr(2), LogicVector::FromInt(0x1000000000000000ULL));

    LogicVector neg = LogicVector::FromInt(0x8000000000000000ULL);
    EXPECT_EQ(neg.asr(1), LogicVector::FromInt(0xC000000000000000ULL));
    EXPECT_EQ(neg.asr(4), LogicVector::FromInt(0xF800000000000000ULL));
    EXPECT_EQ(neg.asr(63), LogicVector::Ones());
    EXPECT_EQ(neg.asr(64), LogicVector::Ones());
    EXPECT_EQ(neg.asr(70), LogicVector::Ones());
}

TEST(LogicVectorTest, ArithmeticShiftRightNarrowWidth)
{
    LogicVector neg8 = LogicVector::FromInt(0x80);
    EXPECT_EQ(neg8.asr(1, 8), LogicVector::FromInt(0xC0));
    EXPECT_EQ(neg8.asr(3, 8), LogicVector::FromInt(0xF0));
    EXPECT_EQ(neg8.asr(7, 8), LogicVector::FromInt(0xFF));
    EXPECT_EQ(neg8.asr(8, 8), LogicVector::FromInt(0xFF));

    LogicVector pos8 = LogicVector::FromInt(0x40);
    EXPECT_EQ(pos8.asr(1, 8), LogicVector::FromInt(0x20));
    EXPECT_EQ(pos8.asr(8, 8), LogicVector::Zero());
}

// ============================================================================
// 9. Rotations (rol, ror)
// ============================================================================

TEST(LogicVectorTest, RotateLeftAndRight64Bit)
{
    LogicVector v = LogicVector::FromInt(0x8000000000000001ULL);

    EXPECT_EQ(v.rol(0), v);
    EXPECT_EQ(v.ror(0), v);

    EXPECT_EQ(v.rol(1), LogicVector::FromInt(0x0000000000000003ULL));
    EXPECT_EQ(v.ror(1), LogicVector::FromInt(0xC000000000000000ULL));

    EXPECT_EQ(v.rol(64), v);
    EXPECT_EQ(v.ror(64), v);

    EXPECT_EQ(v.rol(15).ror(15), v);
    EXPECT_EQ(v.ror(23).rol(23), v);

    EXPECT_EQ(v.rol(-5), v.ror(5));
    EXPECT_EQ(v.ror(-5), v.rol(5));
}

TEST(LogicVectorTest, RotateLeftAndRightNarrowWidth)
{
    LogicVector v8 = LogicVector::FromInt(0x81);

    EXPECT_EQ(v8.rol(1, 8), LogicVector::FromInt(0x03));
    EXPECT_EQ(v8.ror(1, 8), LogicVector::FromInt(0xC0));
    EXPECT_EQ(v8.rol(8, 8), v8);
    EXPECT_EQ(v8.ror(8, 8), v8);
    EXPECT_EQ(v8.rol(9, 8), v8.rol(1, 8));

    LogicVector v4 = LogicVector::FromInt(0x9);
    EXPECT_EQ(v4.rol(1, 4), LogicVector::FromInt(0x3));
    EXPECT_EQ(v4.ror(1, 4), LogicVector::FromInt(0xC));
}

TEST(LogicVectorTest, RotateWithMixedStates)
{
    LogicVector vec = LogicVector::Zero();
    vec.setBit(0, 'X');
    vec.setBit(63, 'Z');

    LogicVector rotated = vec.rol(1);
    EXPECT_EQ(rotated.getBit(1), 'X');
    EXPECT_EQ(rotated.getBit(0), 'Z');
}

// ============================================================================
// 10. Multi-Driver Bus Resolution (resolve)
// ============================================================================

TEST(LogicVectorTest, ResolutionTruthTable)
{
    const char states[] = { '0', '1', 'X', 'Z' };
    const char expectedResolve[4][4] = {
        { '0', 'X', 'X', '0' },
        { 'X', '1', 'X', '1' },
        { 'X', 'X', 'X', 'X' },
        { '0', '1', 'X', 'Z' }
    };

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            LogicVector a = make1Bit(states[i]);
            LogicVector b = make1Bit(states[j]);
            LogicVector res = a.resolve(b);
            EXPECT_EQ(res.getBit(0), expectedResolve[i][j])
                << "Failed resolve for " << states[i] << " resolve " << states[j];
        }
    }
}

TEST(LogicVectorTest, ResolutionVectorMultiBit)
{
    LogicVector driverA{ 0xFFFFFFFF12345678ULL, 0xFFFFFFFF00000000ULL };
    LogicVector driverB{ 0xABCDEF01FFFFFFFFULL, 0x00000000FFFFFFFFULL };

    LogicVector bus = driverA.resolve(driverB);
    EXPECT_EQ(bus.value, 0xABCDEF0112345678ULL);
    EXPECT_EQ(bus.mask, 0ULL);

    LogicVector driverC{ ~0ULL, ~1ULL };
    LogicVector busWithConflict = bus.resolve(driverC);
    EXPECT_EQ(busWithConflict.getBit(0), 'X');
    EXPECT_EQ(busWithConflict.getBit(1), '0');
    EXPECT_EQ(busWithConflict.getBit(2), '0');
    EXPECT_EQ(busWithConflict.getBit(3), '1');
}

TEST(LogicVectorTest, ResolutionAssociativity)
{
    LogicVector a = make1Bit('0');
    LogicVector b = make1Bit('Z');
    LogicVector c = make1Bit('1');

    EXPECT_EQ((a.resolve(b)).resolve(c), a.resolve(b.resolve(c)));
}

TEST(LogicVectorTest, ResolutionIdentity)
{
    LogicVector v = LogicVector::FromInt(0x12345678ULL);
    LogicVector hz = LogicVector::HighZ();

    EXPECT_EQ(v.resolve(hz), v);
    EXPECT_EQ(hz.resolve(v), v);
}

// ============================================================================
// 11. Range Comparison Tests (now possible with methods)
// ============================================================================

TEST(LogicVectorTest, CompareRanges)
{
    LogicVector a = LogicVector::FromInt(0xDEADBEEF01234567ULL);
    LogicVector b = LogicVector::FromInt(0xDEADBEEF01234567ULL);

    // Compare ranges directly (no macro issues)
    EXPECT_EQ(a.getRange(15, 0), b.getRange(15, 0));
    EXPECT_EQ(a.getRange(63, 32), b.getRange(63, 32));
}

TEST(LogicVectorTest, CompareBits)
{
    LogicVector a = LogicVector::FromInt(0xFF);
    LogicVector b = LogicVector::FromInt(0xFF);

    // Compare individual bits directly
    for (uint8_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(a.getBit(i), b.getBit(i));
    }
}

TEST(LogicVectorTest, RangeComparisonWithMixedStates)
{
    LogicVector v = LogicVector::HighZ();
    v.setBit(0, '0');
    v.setBit(1, '1');
    v.setBit(2, 'X');

    LogicVector expected = LogicVector::Zero();
    expected.setBit(0, '0');
    expected.setBit(1, '1');
    expected.setBit(2, 'X');

    EXPECT_EQ(v.getRange(2, 0), expected.getRange(2, 0));
}

// ============================================================================
// 12. Edge Cases and Stress Tests
// ============================================================================

TEST(LogicVectorTest, AllZeroPattern)
{
    LogicVector vec = LogicVector::Zero();
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.getBit(i), '0');
    }

    EXPECT_EQ(vec & vec, vec);
    EXPECT_EQ(vec | vec, vec);
    EXPECT_EQ(vec ^ vec, vec);
    EXPECT_EQ(vec + vec, vec);
}

TEST(LogicVectorTest, AllOnesPattern)
{
    LogicVector vec = LogicVector::Ones();
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.getBit(i), '1');
    }

    EXPECT_EQ(vec & vec, vec);
    EXPECT_EQ(vec | vec, vec);
    EXPECT_EQ(vec ^ vec, LogicVector::Zero());
}

TEST(LogicVectorTest, AlternatingPattern)
{
    LogicVector vec = LogicVector::FromInt(0xAAAAAAAAAAAAAAAAULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        char expected = (i % 2 == 0) ? '0' : '1';
        EXPECT_EQ(vec.getBit(i), expected);
    }

    LogicVector shifted = vec.lsl(1);
    for (uint8_t i = 1; i < 64; ++i)
    {
        char expected = (i % 2 == 0) ? '1' : '0';
        EXPECT_EQ(shifted.getBit(i), expected);
    }
}

TEST(LogicVectorTest, RangeOperationsOnLargeRanges)
{
    LogicVector vec = LogicVector::Zero();

    vec.setRange(63, 32, LogicVector::Ones());

    for (uint8_t i = 32; i < 64; ++i)
    {
        EXPECT_EQ(vec.getBit(i), '1');
    }

    for (uint8_t i = 0; i < 32; ++i)
    {
        EXPECT_EQ(vec.getBit(i), '0');
    }
}

TEST(LogicVectorTest, ComplexOperationSequence)
{
    LogicVector a = LogicVector::FromInt(0x0F0F0F0F0F0F0F0FULL);
    LogicVector b = LogicVector::FromInt(0xF0F0F0F0F0F0F0F0ULL);

    LogicVector result = ((a & b) | (a ^ b)).lsl(4).ror(8);
    EXPECT_EQ(result.getBit(0), '1');
}

TEST(LogicVectorTest, MultipleRangeWrites)
{
    LogicVector vec = LogicVector::Zero();

    vec.setRange(7, 0, LogicVector::FromInt(0xAA));
    vec.setRange(15, 8, LogicVector::FromInt(0x55));
    vec.setRange(23, 16, LogicVector::FromInt(0xFF));

    EXPECT_EQ(vec.getRange(7, 0), LogicVector::FromInt(0xAA));
    EXPECT_EQ(vec.getRange(15, 8), LogicVector::FromInt(0x55));
    EXPECT_EQ(vec.getRange(23, 16), LogicVector::FromInt(0xFF));

    // Write overlapping ranges
    vec.setRange(19, 4, LogicVector::Zero());
    EXPECT_EQ(vec.getBit(4), '0');
    EXPECT_EQ(vec.getBit(19), '0');
}

TEST(LogicVectorTest, RangeExtractionChaining)
{
    LogicVector vec = LogicVector::FromInt(0x123456789ABCDEFULL);

    // Extract nested ranges
    LogicVector r1 = vec.getRange(63, 32);
    LogicVector r2 = r1.getRange(15, 0);
    
    // Should equal the corresponding bits from original
    EXPECT_EQ(r2, vec.getRange(47, 32));
}

TEST(LogicVectorTest, SetRangeMultipleTimes)
{
    LogicVector vec = LogicVector::Zero();

    // Set same range multiple times
    vec.setRange(15, 0, LogicVector::FromInt(0x1111));
    EXPECT_EQ(vec.getRange(15, 0), LogicVector::FromInt(0x1111));

    vec.setRange(15, 0, LogicVector::FromInt(0x2222));
    EXPECT_EQ(vec.getRange(15, 0), LogicVector::FromInt(0x2222));

    vec.setRange(15, 0, LogicVector::FromInt(0x3333));
    EXPECT_EQ(vec.getRange(15, 0), LogicVector::FromInt(0x3333));
}

TEST(LogicVectorTest, BoundaryRanges)
{
    LogicVector vec = LogicVector::FromInt(0xFFFFFFFFFFFFFFFFULL);

    // Bits at boundaries
    EXPECT_EQ(vec.getBit(0), '1');
    EXPECT_EQ(vec.getBit(63), '1');

    // Ranges touching boundaries
    EXPECT_EQ(vec.getRange(0, 0), LogicVector::FromInt(1));
    EXPECT_EQ(vec.getRange(63, 63), LogicVector::FromInt(1));
    EXPECT_EQ(vec.getRange(63, 0), vec);
}