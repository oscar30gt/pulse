#include <gtest/gtest.h>
#include <cstdint>

#include "logicVector.h"

using namespace Pulse;

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
        EXPECT_EQ(vec.get(i), '0');
    }
}

TEST(LogicVectorTest, FactoryOnes)
{
    LogicVector vec = LogicVector::Ones();
    EXPECT_EQ(vec.value, ~0ULL);
    EXPECT_EQ(vec.mask, 0ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.get(i), '1');
    }
}

TEST(LogicVectorTest, FactoryUnknown)
{
    LogicVector vec = LogicVector::Unknown();
    EXPECT_EQ(vec.value, 0ULL);
    EXPECT_EQ(vec.mask, ~0ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.get(i), 'X');
    }
}

TEST(LogicVectorTest, FactoryHighZ)
{
    LogicVector vec = LogicVector::HighZ();
    EXPECT_EQ(vec.value, ~0ULL);
    EXPECT_EQ(vec.mask, ~0ULL);
    for (uint8_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(vec.get(i), 'Z');
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
        EXPECT_EQ(v4.get(i), (i % 2 == 0) ? '1' : '0');
    }
}

TEST(LogicVectorTest, FactoryFromBool)
{
    LogicVector vTrue = LogicVector::FromBool(true);
    EXPECT_EQ(vTrue.value, 1ULL);
    EXPECT_EQ(vTrue.mask, 0ULL);
    EXPECT_EQ(vTrue.get(0), '1');
    EXPECT_EQ(vTrue.get(1), '0');

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
// 3. 4-State Bitwise Operations (AND, OR, XOR, NOT)
// ============================================================================

static LogicVector make1Bit(char state)
{
    LogicVector v{ 0ULL, 0ULL };
    v.set(0, state);
    return v;
}

TEST(LogicVectorTest, BitwiseAndTruthTable)
{
    // Truth table:
    //    0 1 X Z
    //  0 0 0 0 0
    //  1 0 1 X X
    //  X 0 X X X
    //  Z 0 X X X

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
            EXPECT_EQ(res.get(0), expectedAnd[i][j])
                << "Failed AND for " << states[i] << " & " << states[j];
        }
    }
}

TEST(LogicVectorTest, BitwiseOrTruthTable)
{
    // Truth table:
    //    0 1 X Z
    //  0 0 1 X X
    //  1 1 1 1 1
    //  X X 1 X X
    //  Z X 1 X X

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
            EXPECT_EQ(res.get(0), expectedOr[i][j])
                << "Failed OR for " << states[i] << " | " << states[j];
        }
    }
}

TEST(LogicVectorTest, BitwiseXorTruthTable)
{
    // Truth table:
    //    0 1 X Z
    //  0 0 1 X X
    //  1 1 0 X X
    //  X X X X X
    //  Z X X X X

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
            EXPECT_EQ(res.get(0), expectedXor[i][j])
                << "Failed XOR for " << states[i] << " ^ " << states[j];
        }
    }
}

TEST(LogicVectorTest, BitwiseNotTruthTable)
{
    EXPECT_EQ((~make1Bit('0')).get(0), '1');
    EXPECT_EQ((~make1Bit('1')).get(0), '0');
    EXPECT_EQ((~make1Bit('X')).get(0), 'X');
    EXPECT_EQ((~make1Bit('Z')).get(0), 'X');

    EXPECT_EQ(~LogicVector::Zero(), LogicVector::Ones());
    EXPECT_EQ(~LogicVector::Ones(), LogicVector::Zero());

    LogicVector val = LogicVector::FromInt(0x0123456789ABCDEFULL);
    EXPECT_EQ(~(~val), val);
}

TEST(LogicVectorTest, VectorWideBitwiseOperations)
{
    LogicVector a = LogicVector::FromInt(0x0F0F0F0F0F0F0F0FULL);
    LogicVector b = LogicVector::FromInt(0x3333333333333333ULL);

    EXPECT_EQ(a & b, LogicVector::FromInt(0x0303030303030303ULL));
    EXPECT_EQ(a | b, LogicVector::FromInt(0x3F3F3F3F3F3F3F3FULL));
    EXPECT_EQ(a ^ b, LogicVector::FromInt(0x3C3C3C3C3C3C3C3CULL));
}

// ============================================================================
// 4. Arithmetic Addition (operator+)
// ============================================================================

TEST(LogicVectorTest, AdditionDefiniteValues)
{
    LogicVector a = LogicVector::FromInt(40);
    LogicVector b = LogicVector::FromInt(2);
    EXPECT_EQ(a + b, LogicVector::FromInt(42));

    LogicVector maxVal = LogicVector::FromInt(~0ULL);
    LogicVector one = LogicVector::FromInt(1);
    EXPECT_EQ(maxVal + one, LogicVector::FromInt(0ULL)); // 64-bit wrap-around
}

TEST(LogicVectorTest, AdditionWithIndefiniteValuesReturnsUnknown)
{
    LogicVector def = LogicVector::FromInt(42);
    LogicVector unk = LogicVector::Unknown();
    LogicVector hz = LogicVector::HighZ();

    EXPECT_EQ(def + unk, LogicVector::Unknown());
    EXPECT_EQ(unk + def, LogicVector::Unknown());
    EXPECT_EQ(def + hz, LogicVector::Unknown());
    EXPECT_EQ(unk + hz, LogicVector::Unknown());

    LogicVector mixed{ 1ULL, 2ULL }; // bit 1 is indefinite
    EXPECT_EQ(def + mixed, LogicVector::Unknown());
}

// ============================================================================
// 5. Logical & Arithmetic Shifts (lsl, lsr, asr)
// ============================================================================

TEST(LogicVectorTest, LogicalShiftLeft)
{
    LogicVector v = LogicVector::FromInt(0x000000000000000FULL);

    EXPECT_EQ(v.lsl(0), v);
    EXPECT_EQ(v.lsl(4), LogicVector::FromInt(0x00000000000000F0ULL));
    EXPECT_EQ(v.lsl(60), LogicVector::FromInt(0xF000000000000000ULL));
    EXPECT_EQ(v.lsl(64), LogicVector::Zero());
    EXPECT_EQ(v.lsl(70), LogicVector::Zero());

    // Negative shamt acts as LSR
    EXPECT_EQ(v.lsl(-2), LogicVector::FromInt(0x0000000000000003ULL));

    // Custom width (8-bit)
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

    // Negative shamt acts as LSL
    LogicVector small = LogicVector::FromInt(0x01);
    EXPECT_EQ(small.lsr(-4), LogicVector::FromInt(0x10));

    // Custom width (8-bit)
    LogicVector v8 = LogicVector::FromInt(0xF0);
    EXPECT_EQ(v8.lsr(2, 8), LogicVector::FromInt(0x3C));
}

TEST(LogicVectorTest, ArithmeticShiftRight)
{
    // Positive number (MSB is 0)
    LogicVector pos = LogicVector::FromInt(0x4000000000000000ULL);
    EXPECT_EQ(pos.asr(2), LogicVector::FromInt(0x1000000000000000ULL));

    // Negative number (MSB is 1)
    LogicVector neg = LogicVector::FromInt(0x8000000000000000ULL);
    EXPECT_EQ(neg.asr(1), LogicVector::FromInt(0xC000000000000000ULL));
    EXPECT_EQ(neg.asr(4), LogicVector::FromInt(0xF800000000000000ULL));
    EXPECT_EQ(neg.asr(63), LogicVector::Ones());
    EXPECT_EQ(neg.asr(64), LogicVector::Ones());
    EXPECT_EQ(neg.asr(70), LogicVector::Ones());

    // Custom width (8-bit signed)
    LogicVector neg8 = LogicVector::FromInt(0x80); // 8-bit sign bit set
    EXPECT_EQ(neg8.asr(1, 8), LogicVector::FromInt(0xC0));
    EXPECT_EQ(neg8.asr(3, 8), LogicVector::FromInt(0xF0));
    EXPECT_EQ(neg8.asr(7, 8), LogicVector::FromInt(0xFF));
    EXPECT_EQ(neg8.asr(8, 8), LogicVector::FromInt(0xFF));

    LogicVector pos8 = LogicVector::FromInt(0x40); // 8-bit sign bit 0
    EXPECT_EQ(pos8.asr(1, 8), LogicVector::FromInt(0x20));
    EXPECT_EQ(pos8.asr(8, 8), LogicVector::Zero());

    // Negative shamt acts as LSL
    EXPECT_EQ(pos8.asr(-2, 8), LogicVector::FromInt(0x00));
}

// ============================================================================
// 6. Rotations (rol, ror)
// ============================================================================

TEST(LogicVectorTest, RotateLeftAndRight64Bit)
{
    LogicVector v = LogicVector::FromInt(0x8000000000000001ULL);

    EXPECT_EQ(v.rol(0), v);
    EXPECT_EQ(v.ror(0), v);

    // ROL 1: bit 63 becomes bit 0, bit 0 becomes bit 1
    EXPECT_EQ(v.rol(1), LogicVector::FromInt(0x0000000000000003ULL));

    // ROR 1: bit 0 becomes bit 63, bit 63 becomes bit 62
    EXPECT_EQ(v.ror(1), LogicVector::FromInt(0xC000000000000000ULL));

    // Full circle rotation
    EXPECT_EQ(v.rol(64), v);
    EXPECT_EQ(v.ror(64), v);

    // Invertibility
    EXPECT_EQ(v.rol(15).ror(15), v);
    EXPECT_EQ(v.ror(23).rol(23), v);

    // Negative shamt
    EXPECT_EQ(v.rol(-5), v.ror(5));
    EXPECT_EQ(v.ror(-5), v.rol(5));
}

TEST(LogicVectorTest, RotateLeftAndRightNarrowWidth)
{
    // 8-bit rotation of 0x81 (1000 0001)
    LogicVector v8 = LogicVector::FromInt(0x81);

    EXPECT_EQ(v8.rol(1, 8), LogicVector::FromInt(0x03)); // 0000 0011
    EXPECT_EQ(v8.ror(1, 8), LogicVector::FromInt(0xC0)); // 1100 0000
    EXPECT_EQ(v8.rol(8, 8), v8);
    EXPECT_EQ(v8.ror(8, 8), v8);
    EXPECT_EQ(v8.rol(9, 8), v8.rol(1, 8));

    // 4-bit rotation of 0x9 (1001)
    LogicVector v4 = LogicVector::FromInt(0x9);
    EXPECT_EQ(v4.rol(1, 4), LogicVector::FromInt(0x3));
    EXPECT_EQ(v4.ror(1, 4), LogicVector::FromInt(0xC));
}

// ============================================================================
// 7. Multi-Driver Bus Resolution (resolve)
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
            EXPECT_EQ(res.get(0), expectedResolve[i][j])
                << "Failed resolve for " << states[i] << " resolve " << states[j];
        }
    }
}

TEST(LogicVectorTest, ResolutionVectorMultiBit)
{
    // Tri-state bus multiplexing:
    // Driver A active on lower 32 bits, High-Z (v=1, m=1) on upper 32 bits
    LogicVector driverA{ 0xFFFFFFFF12345678ULL, 0xFFFFFFFF00000000ULL };
    // Driver B High-Z (v=1, m=1) on lower 32 bits, active on upper 32 bits
    LogicVector driverB{ 0xABCDEF01FFFFFFFFULL, 0x00000000FFFFFFFFULL };

    LogicVector bus = driverA.resolve(driverB);
    EXPECT_EQ(bus.value, 0xABCDEF0112345678ULL);
    EXPECT_EQ(bus.mask, 0ULL); // All bits definite!

    // Add driver with contention on bit 0 only (bit 0 is driven to 1, bits 1..63 are High-Z)
    LogicVector driverC{ ~0ULL, ~1ULL };
    LogicVector busWithConflict = bus.resolve(driverC);
    // Bit 0 was 0 in driverA (0x...8), now driverC drives 1 -> contention on bit 0!
    EXPECT_EQ(busWithConflict.get(0), 'X');
    EXPECT_EQ(busWithConflict.get(1), '0');
    EXPECT_EQ(busWithConflict.get(2), '0');
    EXPECT_EQ(busWithConflict.get(3), '1');
}

// ============================================================================
// 8. Bit Manipulation & Range Extraction (get, set, getRange)
// ============================================================================

TEST(LogicVectorTest, BitGetAndSet)
{
    LogicVector vec = LogicVector::Zero();

    vec.set(0, '1');
    vec.set(5, 'X');
    vec.set(63, 'Z');

    EXPECT_EQ(vec.get(0), '1');
    EXPECT_EQ(vec.get(1), '0');
    EXPECT_EQ(vec.get(5), 'X');
    EXPECT_EQ(vec.get(63), 'Z');

    // Overwrite existing states
    vec.set(0, '0');
    EXPECT_EQ(vec.get(0), '0');
    vec.set(5, '1');
    EXPECT_EQ(vec.get(5), '1');
    vec.set(63, 'X');
    EXPECT_EQ(vec.get(63), 'X');

    // Out of bounds get and set
    EXPECT_EQ(vec.get(64), 0);
    EXPECT_EQ(vec.get(100), 0);

    // Setting out of bounds or invalid char does not crash or corrupt
    vec.set(64, '1');
    vec.set(10, '?');
    EXPECT_EQ(vec.get(10), '0');
}

TEST(LogicVectorTest, GetRangeValid)
{
    // 0xDEADBEEF01234567
    LogicVector vec = LogicVector::FromInt(0xDEADBEEF01234567ULL);

    // Lowest byte (bits 0..7): 0x67
    LogicVector byte0 = vec.getRange(0, 7);
    EXPECT_EQ(byte0, LogicVector::FromInt(0x67));

    // Second byte (bits 8..15): 0x45
    LogicVector byte1 = vec.getRange(8, 15);
    EXPECT_EQ(byte1, LogicVector::FromInt(0x45));

    // Upper 32 bits (bits 32..63): 0xDEADBEEF
    LogicVector upper32 = vec.getRange(32, 63);
    EXPECT_EQ(upper32, LogicVector::FromInt(0xDEADBEEFULL));

    // Single bit range
    EXPECT_EQ(vec.getRange(0, 0), LogicVector::FromInt(1)); // 0x7 & 1 = 1
    EXPECT_EQ(vec.getRange(1, 1), LogicVector::FromInt(1)); // (0x7 >> 1) & 1 = 1
    EXPECT_EQ(vec.getRange(2, 2), LogicVector::FromInt(1)); // (0x7 >> 2) & 1 = 1
    EXPECT_EQ(vec.getRange(3, 3), LogicVector::FromInt(0)); // (0x7 >> 3) & 1 = 0

    // Full 64-bit range
    EXPECT_EQ(vec.getRange(0, 63), vec);
}

TEST(LogicVectorTest, GetRangeWithMixedStates)
{
    LogicVector vec = LogicVector::HighZ(); // All bits Z
    vec.set(10, '0');
    vec.set(11, '1');
    vec.set(12, 'X');
    vec.set(13, 'Z');

    LogicVector sub = vec.getRange(10, 13);
    EXPECT_EQ(sub.get(0), '0');
    EXPECT_EQ(sub.get(1), '1');
    EXPECT_EQ(sub.get(2), 'X');
    EXPECT_EQ(sub.get(3), 'Z');
}

TEST(LogicVectorTest, GetRangeInvalidBounds)
{
    LogicVector vec = LogicVector::FromInt(0x1234);

    EXPECT_EQ(vec.getRange(10, 5), LogicVector::Unknown()); // start > end
    EXPECT_EQ(vec.getRange(64, 65), LogicVector::Unknown()); // out of bounds
    EXPECT_EQ(vec.getRange(0, 64), LogicVector::Unknown()); // end out of bounds
}
