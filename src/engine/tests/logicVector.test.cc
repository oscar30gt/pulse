#include <gtest/gtest.h>
#include "logicVector.h"

#include <limits>
#include <stdexcept>

using Pulse::Engine::LogicVector;

namespace
{
    // Builds a single-bit (bit 0) LogicVector representing one of the four
    // IEEE 1164 states, for exhaustive truth-table testing.
    LogicVector StateBit(char state)
    {
        switch (state)
        {
            case '0': return LogicVector{ 0ULL, 0ULL };
            case '1': return LogicVector{ 1ULL, 0ULL };
            case 'X': return LogicVector{ 0ULL, 1ULL };
            case 'Z': return LogicVector{ 1ULL, 1ULL };
            default: throw std::invalid_argument("bad state");
        }
    }

    // Extracts the resulting state character of bit 0 from a LogicVector
    // built purely from bit-0 value/mask (helper mirrors LogicVector::bit()
    // semantics without depending on the method under test where possible).
    char StateOf(const LogicVector& lv)
    {
        return lv.bit(0);
    }

    constexpr char kStates[4] = { '0', '1', 'X', 'Z' };
}

// ============================================================================
// Construction
// ============================================================================

TEST(Construction, DefaultConstructorIsHighZ)
{
    LogicVector lv;
    EXPECT_EQ(lv.value, ~0ULL);
    EXPECT_EQ(lv.mask, ~0ULL);
    EXPECT_EQ(lv, LogicVector::HighZ());
}

TEST(Construction, ValueOnlyConstructorDefaultsMaskToZero)
{
    LogicVector lv(42ULL);
    EXPECT_EQ(lv.value, 42ULL);
    EXPECT_EQ(lv.mask, 0ULL);
    EXPECT_TRUE(lv.isDefinite());
}

TEST(Construction, ValueAndMaskConstructor)
{
    LogicVector lv(0xABCDULL, 0x0F0FULL);
    EXPECT_EQ(lv.value, 0xABCDULL);
    EXPECT_EQ(lv.mask, 0x0F0FULL);
}

TEST(Construction, ConstexprUsability)
{
    constexpr LogicVector lv(5ULL, 0ULL);
    static_assert(lv.value == 5ULL, "constexpr construction failed");
    SUCCEED();
}

// ============================================================================
// Factory methods
// ============================================================================

TEST(FactoryMethods, Zero)
{
    LogicVector lv = LogicVector::Zero();
    EXPECT_EQ(lv.value, 0ULL);
    EXPECT_EQ(lv.mask, 0ULL);
    EXPECT_EQ(lv.str(4), "0000");
}

TEST(FactoryMethods, Ones)
{
    LogicVector lv = LogicVector::Ones();
    EXPECT_EQ(lv.value, ~0ULL);
    EXPECT_EQ(lv.mask, 0ULL);
    EXPECT_EQ(lv.str(4), "1111");
}

TEST(FactoryMethods, Unknown)
{
    LogicVector lv = LogicVector::Unknown();
    EXPECT_EQ(lv.value, 0ULL);
    EXPECT_EQ(lv.mask, ~0ULL);
    EXPECT_EQ(lv.str(4), "XXXX");
}

TEST(FactoryMethods, HighZ)
{
    LogicVector lv = LogicVector::HighZ();
    EXPECT_EQ(lv.value, ~0ULL);
    EXPECT_EQ(lv.mask, ~0ULL);
    EXPECT_EQ(lv.str(4), "ZZZZ");
}

TEST(FactoryMethods, FromIntZero)
{
    LogicVector lv = LogicVector::FromInt(0ULL);
    EXPECT_EQ(lv, LogicVector::Zero());
}

TEST(FactoryMethods, FromIntMax)
{
    LogicVector lv = LogicVector::FromInt(~0ULL);
    EXPECT_EQ(lv.value, ~0ULL);
    EXPECT_EQ(lv.mask, 0ULL);
}

TEST(FactoryMethods, FromIntArbitrary)
{
    LogicVector lv = LogicVector::FromInt(0x123456789ABCDEFULL);
    EXPECT_EQ(lv.value, 0x123456789ABCDEFULL);
    EXPECT_EQ(lv.mask, 0ULL);
}

TEST(FactoryMethods, FromBoolTrue)
{
    LogicVector lv = LogicVector::FromBool(true);
    EXPECT_EQ(lv.value, 1ULL);
    EXPECT_EQ(lv.mask, 0ULL);
}

TEST(FactoryMethods, FromBoolFalse)
{
    LogicVector lv = LogicVector::FromBool(false);
    EXPECT_EQ(lv, LogicVector::Zero());
}

// ============================================================================
// Conversion operator: bool
// ============================================================================

TEST(ConvertBool, ZeroIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(LogicVector::Zero()));
}

TEST(ConvertBool, OnesIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(LogicVector::Ones()));
}

TEST(ConvertBool, UnknownIsTrue)
{
    // value == 0 but mask != 0, so (value | mask) != 0 -> true.
    EXPECT_TRUE(static_cast<bool>(LogicVector::Unknown()));
}

TEST(ConvertBool, HighZIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(LogicVector::HighZ()));
}

TEST(ConvertBool, SingleSetValueBitIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(LogicVector::FromInt(1ULL)));
}

TEST(ConvertBool, SingleSetMaskBitOnlyIsTrue)
{
    LogicVector lv(0ULL, 1ULL); // one X bit, rest '0'
    EXPECT_TRUE(static_cast<bool>(lv));
}

// ============================================================================
// Conversion operators: unsigned integers
// ============================================================================

TEST(ConvertUnsigned, Uint64RoundTrip)
{
    LogicVector lv = LogicVector::FromInt(0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(static_cast<uint64_t>(lv), 0xDEADBEEFCAFEBABEULL);
}

TEST(ConvertUnsigned, Uint64ThrowsOnAnyUndefinedBit)
{
    LogicVector lv(0ULL, 1ULL); // bit0 = X
    EXPECT_THROW(static_cast<uint64_t>(lv), std::runtime_error);
}

TEST(ConvertUnsigned, Uint64ThrowsOnHighZ)
{
    EXPECT_THROW(static_cast<uint64_t>(LogicVector::HighZ()), std::runtime_error);
}

TEST(ConvertUnsigned, Uint32TruncatesUpperBits)
{
    // Upper 32 bits set but definite; lower 32 bits are 0x1.
    LogicVector lv = LogicVector::FromInt(0xFFFFFFFF00000001ULL);
    EXPECT_EQ(static_cast<uint32_t>(lv), 0x00000001u);
}

TEST(ConvertUnsigned, Uint32ThrowsIfUndefinedBitsBeyond32)
{
    // All mask bits above bit 31 are set even though the low 32 bits are
    // fully definite - the whole vector's mask is checked, not just the
    // truncated portion.
    LogicVector lv(0x1ULL, 0xFFFFFFFF00000000ULL);
    EXPECT_THROW(static_cast<uint32_t>(lv), std::runtime_error);
}

TEST(ConvertUnsigned, Uint16Truncates)
{
    LogicVector lv = LogicVector::FromInt(0x1234ABCDULL);
    EXPECT_EQ(static_cast<uint16_t>(lv), 0xABCDu);
}

TEST(ConvertUnsigned, Uint8Truncates)
{
    LogicVector lv = LogicVector::FromInt(0x1FFULL);
    EXPECT_EQ(static_cast<uint8_t>(lv), 0xFFu);
}

TEST(ConvertUnsigned, Uint8ThrowsWhenAnyBitUndefined)
{
    LogicVector lv(0xFFULL, 0x8000000000000000ULL); // one X bit far away
    EXPECT_THROW(static_cast<uint8_t>(lv), std::runtime_error);
}

// ============================================================================
// Conversion operators: signed integers
// ============================================================================

TEST(ConvertSigned, Int64AllOnesIsMinusOne)
{
    LogicVector lv = LogicVector::FromInt(~0ULL);
    EXPECT_EQ(static_cast<int64_t>(lv), -1);
}

TEST(ConvertSigned, Int64ThrowsOnUndefined)
{
    EXPECT_THROW(static_cast<int64_t>(LogicVector::Unknown()), std::runtime_error);
}

TEST(ConvertSigned, Int32NegativeValue)
{
    LogicVector lv = LogicVector::FromInt(0xFFFFFFFFULL); // low 32 bits all 1
    EXPECT_EQ(static_cast<int32_t>(lv), -1);
}

TEST(ConvertSigned, Int16NegativeValue)
{
    LogicVector lv = LogicVector::FromInt(0xFFFFULL);
    EXPECT_EQ(static_cast<int16_t>(lv), -1);
}

TEST(ConvertSigned, Int8NegativeValue)
{
    LogicVector lv = LogicVector::FromInt(0xFFULL);
    EXPECT_EQ(static_cast<int8_t>(lv), -1);
}

TEST(ConvertSigned, Int8PositiveValue)
{
    LogicVector lv = LogicVector::FromInt(0x7FULL);
    EXPECT_EQ(static_cast<int8_t>(lv), 127);
}

TEST(ConvertSigned, Int8ThrowsOnUndefined)
{
    LogicVector lv(0x0ULL, 0x1ULL);
    EXPECT_THROW(static_cast<int8_t>(lv), std::runtime_error);
}

// ============================================================================
// Equality operators
// ============================================================================

TEST(Equality, SameValueSameMaskIsEqual)
{
    LogicVector a(5ULL, 3ULL);
    LogicVector b(5ULL, 3ULL);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(Equality, DifferentValueIsNotEqual)
{
    LogicVector a(5ULL, 0ULL);
    LogicVector b(6ULL, 0ULL);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(Equality, DifferentMaskIsNotEqual)
{
    // Same "value" field but different mask means different logical state
    // per-bit (e.g. value=1,mask=0 is '1' but value=1,mask=1 is 'Z').
    LogicVector a(1ULL, 0ULL);
    LogicVector b(1ULL, 1ULL);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(Equality, SelfEquality)
{
    LogicVector a = LogicVector::HighZ();
    EXPECT_TRUE(a == a);
}

TEST(Equality, DefaultEqualsHighZ)
{
    EXPECT_EQ(LogicVector(), LogicVector::HighZ());
}

// ============================================================================
// Bitwise AND - exhaustive 4x4 truth table (per IEEE 1164 §7.2.1) + word level
// ============================================================================

class AndTruthTable : public ::testing::TestWithParam<std::tuple<char, char, char>> {};

TEST_P(AndTruthTable, MatchesIeee1164)
{
    auto [a, b, expected] = GetParam();
    LogicVector result = StateBit(a) & StateBit(b);
    EXPECT_EQ(StateOf(result), expected) << "a=" << a << " b=" << b;
}

INSTANTIATE_TEST_SUITE_P(
    Ieee1164, AndTruthTable,
    ::testing::Values(
        std::make_tuple('0', '0', '0'), std::make_tuple('0', '1', '0'),
        std::make_tuple('0', 'X', '0'), std::make_tuple('0', 'Z', '0'),
        std::make_tuple('1', '0', '0'), std::make_tuple('1', '1', '1'),
        std::make_tuple('1', 'X', 'X'), std::make_tuple('1', 'Z', 'X'),
        std::make_tuple('X', '0', '0'), std::make_tuple('X', '1', 'X'),
        std::make_tuple('X', 'X', 'X'), std::make_tuple('X', 'Z', 'X'),
        std::make_tuple('Z', '0', '0'), std::make_tuple('Z', '1', 'X'),
        std::make_tuple('Z', 'X', 'X'), std::make_tuple('Z', 'Z', 'X')
    )
);

TEST(BitwiseAnd, WordLevel)
{
    LogicVector a = LogicVector::FromInt(0b1100);
    LogicVector b = LogicVector::FromInt(0b1010);
    LogicVector r = a & b;
    EXPECT_EQ(r.value, 0b1000ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(BitwiseAnd, IsCommutative)
{
    LogicVector a(0b1101ULL, 0b0010ULL);
    LogicVector b(0b0110ULL, 0b1000ULL);
    EXPECT_EQ(a & b, b & a);
}

TEST(BitwiseAnd, WithZeroIsZero)
{
    LogicVector a = LogicVector::HighZ();
    LogicVector r = a & LogicVector::Zero();
    EXPECT_EQ(r, LogicVector::Zero());
}

// ============================================================================
// Bitwise OR - exhaustive 4x4 truth table
// ============================================================================

class OrTruthTable : public ::testing::TestWithParam<std::tuple<char, char, char>> {};

TEST_P(OrTruthTable, MatchesIeee1164)
{
    auto [a, b, expected] = GetParam();
    LogicVector result = StateBit(a) | StateBit(b);
    EXPECT_EQ(StateOf(result), expected) << "a=" << a << " b=" << b;
}

INSTANTIATE_TEST_SUITE_P(
    Ieee1164, OrTruthTable,
    ::testing::Values(
        std::make_tuple('0', '0', '0'), std::make_tuple('0', '1', '1'),
        std::make_tuple('0', 'X', 'X'), std::make_tuple('0', 'Z', 'X'),
        std::make_tuple('1', '0', '1'), std::make_tuple('1', '1', '1'),
        std::make_tuple('1', 'X', '1'), std::make_tuple('1', 'Z', '1'),
        std::make_tuple('X', '0', 'X'), std::make_tuple('X', '1', '1'),
        std::make_tuple('X', 'X', 'X'), std::make_tuple('X', 'Z', 'X'),
        std::make_tuple('Z', '0', 'X'), std::make_tuple('Z', '1', '1'),
        std::make_tuple('Z', 'X', 'X'), std::make_tuple('Z', 'Z', 'X')
    )
);

TEST(BitwiseOr, WordLevel)
{
    LogicVector a = LogicVector::FromInt(0b1100);
    LogicVector b = LogicVector::FromInt(0b1010);
    LogicVector r = a | b;
    EXPECT_EQ(r.value, 0b1110ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(BitwiseOr, WithOnesIsOnes)
{
    LogicVector r = LogicVector::HighZ() | LogicVector::Ones();
    EXPECT_EQ(r, LogicVector::Ones());
}

// ============================================================================
// Bitwise XOR - exhaustive 4x4 truth table
// ============================================================================

class XorTruthTable : public ::testing::TestWithParam<std::tuple<char, char, char>> {};

TEST_P(XorTruthTable, MatchesIeee1164)
{
    auto [a, b, expected] = GetParam();
    LogicVector result = StateBit(a) ^ StateBit(b);
    EXPECT_EQ(StateOf(result), expected) << "a=" << a << " b=" << b;
}

INSTANTIATE_TEST_SUITE_P(
    Ieee1164, XorTruthTable,
    ::testing::Values(
        std::make_tuple('0', '0', '0'), std::make_tuple('0', '1', '1'),
        std::make_tuple('0', 'X', 'X'), std::make_tuple('0', 'Z', 'X'),
        std::make_tuple('1', '0', '1'), std::make_tuple('1', '1', '0'),
        std::make_tuple('1', 'X', 'X'), std::make_tuple('1', 'Z', 'X'),
        std::make_tuple('X', '0', 'X'), std::make_tuple('X', '1', 'X'),
        std::make_tuple('X', 'X', 'X'), std::make_tuple('X', 'Z', 'X'),
        std::make_tuple('Z', '0', 'X'), std::make_tuple('Z', '1', 'X'),
        std::make_tuple('Z', 'X', 'X'), std::make_tuple('Z', 'Z', 'X')
    )
);

TEST(BitwiseXor, WordLevel)
{
    LogicVector a = LogicVector::FromInt(0b1100);
    LogicVector b = LogicVector::FromInt(0b1010);
    LogicVector r = a ^ b;
    EXPECT_EQ(r.value, 0b0110ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(BitwiseXor, SelfXorIsZeroWhenDefinite)
{
    LogicVector a = LogicVector::FromInt(0x12345678ULL);
    EXPECT_EQ(a ^ a, LogicVector::Zero());
}

// ============================================================================
// Bitwise NOT - truth table
// ============================================================================

class NotTruthTable : public ::testing::TestWithParam<std::tuple<char, char>> {};

TEST_P(NotTruthTable, MatchesIeee1164)
{
    auto [a, expected] = GetParam();
    LogicVector result = ~StateBit(a);
    EXPECT_EQ(StateOf(result), expected) << "a=" << a;
}

INSTANTIATE_TEST_SUITE_P(
    Ieee1164, NotTruthTable,
    ::testing::Values(
        std::make_tuple('0', '1'), std::make_tuple('1', '0'),
        std::make_tuple('X', 'X'), std::make_tuple('Z', 'X')
    )
);

TEST(BitwiseNot, WordLevel)
{
    LogicVector a = LogicVector::FromInt(0b1010);
    LogicVector r = ~a;
    // Only low 4 bits matter for the readable pattern, upper bits also flip.
    EXPECT_EQ(r.value, ~0b1010ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(BitwiseNot, DoubleNegationIsIdentityWhenDefinite)
{
    LogicVector a = LogicVector::FromInt(0xC0FFEEULL);
    EXPECT_EQ(~(~a), a);
}

TEST(BitwiseNot, NotOfUnknownIsUnknown)
{
    EXPECT_EQ(~LogicVector::Unknown(), LogicVector::Unknown());
}

TEST(BitwiseNot, NotOfHighZBecomesUnknownNotHighZ)
{
    LogicVector r = ~LogicVector::HighZ();
    EXPECT_EQ(r, LogicVector::Unknown());
    EXPECT_NE(r, LogicVector::HighZ());
}

// ============================================================================
// Addition, Subtraction, Multiplication
// ============================================================================

TEST(Addition, BothDefiniteIsSum)
{
    LogicVector a = LogicVector::FromInt(7ULL);
    LogicVector b = LogicVector::FromInt(5ULL);
    LogicVector r = a + b;
    EXPECT_EQ(r.value, 12ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(Addition, ZeroPlusZeroIsZero)
{
    EXPECT_EQ(LogicVector::Zero() + LogicVector::Zero(), LogicVector::Zero());
}

TEST(Addition, WrapsOnOverflow)
{
    LogicVector a = LogicVector::FromInt(~0ULL);   // all ones
    LogicVector b = LogicVector::FromInt(1ULL);
    LogicVector r = a + b;
    EXPECT_EQ(r.value, 0ULL); // wraps mod 2^64
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(Addition, AnyUndefinedBitInLhsYieldsFullUnknown)
{
    LogicVector a(0b1ULL, 0b1ULL); // only bit 0 is X, rest definite
    LogicVector b = LogicVector::FromInt(5ULL);
    LogicVector r = a + b;
    EXPECT_EQ(r, LogicVector::Unknown());
}

TEST(Addition, AnyUndefinedBitInRhsYieldsFullUnknown)
{
    LogicVector a = LogicVector::FromInt(5ULL);
    LogicVector b(0ULL, 0x8000000000000000ULL); // single high X bit
    LogicVector r = a + b;
    EXPECT_EQ(r, LogicVector::Unknown());
}

TEST(Addition, IsCommutativeWhenDefinite)
{
    LogicVector a = LogicVector::FromInt(123ULL);
    LogicVector b = LogicVector::FromInt(45ULL);
    EXPECT_EQ(a + b, b + a);
}

TEST(Subtraction, BothDefiniteIsDifference)
{
    LogicVector a = LogicVector::FromInt(10ULL);
    LogicVector b = LogicVector::FromInt(3ULL);
    LogicVector r = a - b;
    EXPECT_EQ(r.value, 7ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(Subtraction, SelfMinusSelfIsZero)
{
    LogicVector a = LogicVector::FromInt(999ULL);
    EXPECT_EQ(a - a, LogicVector::Zero());
}

TEST(Subtraction, WrapsOnUnderflow)
{
    LogicVector a = LogicVector::Zero();
    LogicVector b = LogicVector::FromInt(1ULL);
    LogicVector r = a - b;
    EXPECT_EQ(r.value, ~0ULL); // wraps mod 2^64
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(Subtraction, AnyUndefinedBitInLhsYieldsFullUnknown)
{
    LogicVector a(0b1ULL, 0b1ULL);
    LogicVector b = LogicVector::FromInt(5ULL);
    LogicVector r = a - b;
    EXPECT_EQ(r, LogicVector::Unknown());
}

TEST(Subtraction, AnyUndefinedBitInRhsYieldsFullUnknown)
{
    LogicVector a = LogicVector::FromInt(5ULL);
    LogicVector b(0ULL, 0x8000000000000000ULL);
    LogicVector r = a - b;
    EXPECT_EQ(r, LogicVector::Unknown());
}

TEST(Multiplication, BothDefiniteIsProduct)
{
    LogicVector a = LogicVector::FromInt(6ULL);
    LogicVector b = LogicVector::FromInt(7ULL);
    LogicVector r = a * b;
    EXPECT_EQ(r.value, 42ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(Multiplication, AnythingTimesZeroIsZero)
{
    LogicVector a = LogicVector::FromInt(123456ULL);
    LogicVector r = a * LogicVector::Zero();
    EXPECT_EQ(r, LogicVector::Zero());
}

TEST(Multiplication, WrapsOnOverflow)
{
    LogicVector a = LogicVector::FromInt(1ULL << 40);
    LogicVector b = LogicVector::FromInt(1ULL << 40);
    LogicVector r = a * b;
    EXPECT_EQ(r.value, (1ULL << 40) * (1ULL << 40)); // implicit mod 2^64 wrap
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(Multiplication, AnyUndefinedBitInLhsYieldsFullUnknown)
{
    LogicVector a(0b1ULL, 0b1ULL);
    LogicVector b = LogicVector::FromInt(5ULL);
    LogicVector r = a * b;
    EXPECT_EQ(r, LogicVector::Unknown());
}

TEST(Multiplication, AnyUndefinedBitInRhsYieldsFullUnknown)
{
    LogicVector a = LogicVector::FromInt(5ULL);
    LogicVector b(0ULL, 0x8000000000000000ULL);
    LogicVector r = a * b;
    EXPECT_EQ(r, LogicVector::Unknown());
}

TEST(Multiplication, IsCommutativeWhenDefinite)
{
    LogicVector a = LogicVector::FromInt(123ULL);
    LogicVector b = LogicVector::FromInt(45ULL);
    EXPECT_EQ(a * b, b * a);
}

// ============================================================================
// Shift: lsl (logical shift left)
// ============================================================================

TEST(ShiftLsl, ShamtZeroIsIdentityWithinWidth)
{
    LogicVector a = LogicVector::FromInt(0b1011);
    LogicVector r = a.lsl(0, 4);
    EXPECT_EQ(r.value, 0b1011ULL);
}

TEST(ShiftLsl, BasicShift)
{
    LogicVector a = LogicVector::FromInt(0b0001);
    LogicVector r = a.lsl(3, 8);
    EXPECT_EQ(r.value, 0b00001000ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(ShiftLsl, ShamtEqualsWidthShiftsOutEverything)
{
    LogicVector a = LogicVector::FromInt(0xFF);
    LogicVector r = a.lsl(8, 8);
    EXPECT_EQ(r.value, 0ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(ShiftLsl, ShamtGreaterThanWidthShiftsOutEverything)
{
    LogicVector a = LogicVector::FromInt(0xFF);
    LogicVector r = a.lsl(200, 8);
    EXPECT_EQ(r, LogicVector(0ULL, 0ULL));
}

TEST(ShiftLsl, ShamtOf64ShiftsOutEverything)
{
    LogicVector a = LogicVector::Ones();
    LogicVector r = a.lsl(64); // default width 64, shamt >= 64
    EXPECT_EQ(r, LogicVector(0ULL, 0ULL));
}

TEST(ShiftLsl, WidthZeroAlwaysShiftsOutEverything)
{
    LogicVector a = LogicVector::FromInt(0xFFFFFFFFFFFFFFFFULL);
    LogicVector r = a.lsl(0, 0);
    EXPECT_EQ(r, LogicVector(0ULL, 0ULL));
}

TEST(ShiftLsl, MasksToWidthBeforeShifting)
{
    // Bits above the given width are dropped even before shifting.
    LogicVector a = LogicVector::FromInt(0xFF); // 8 bits set
    LogicVector r = a.lsl(1, 4);                // only low 4 bits considered
    EXPECT_EQ(r.value, 0b11110ULL);
}

TEST(ShiftLsl, PropagatesMaskBits)
{
    LogicVector a(0b0001ULL, 0b0010ULL); // bit0='1', bit1='X'
    LogicVector r = a.lsl(2, 8);
    EXPECT_EQ(r.value, 0b000100ULL);
    EXPECT_EQ(r.mask, 0b001000ULL);
}

TEST(ShiftLsl, DefaultWidthIs64)
{
    LogicVector a = LogicVector::FromInt(1ULL);
    LogicVector r = a.lsl(63);
    EXPECT_EQ(r.value, 0x8000000000000000ULL);
}

// ============================================================================
// Shift: lsr (logical shift right)
// ============================================================================

TEST(ShiftLsr, BasicShift)
{
    LogicVector a = LogicVector::FromInt(0xFF);
    LogicVector r = a.lsr(4, 8);
    EXPECT_EQ(r.value, 0x0FULL);
}

TEST(ShiftLsr, ShamtEqualsWidthShiftsOutEverything)
{
    LogicVector a = LogicVector::FromInt(0xFF);
    LogicVector r = a.lsr(8, 8);
    EXPECT_EQ(r, LogicVector(0ULL, 0ULL));
}

TEST(ShiftLsr, ShamtGreaterThan64ShiftsOutEverything)
{
    LogicVector a = LogicVector::Ones();
    LogicVector r = a.lsr(255);
    EXPECT_EQ(r, LogicVector(0ULL, 0ULL));
}

TEST(ShiftLsr, DoesNotSignExtend)
{
    LogicVector a = LogicVector::FromInt(0x80); // bit7 set, width 8
    LogicVector r = a.lsr(4, 8);
    EXPECT_EQ(r.value, 0x08ULL); // zero-filled from the left, not sign-extended
}

TEST(ShiftLsr, PropagatesMaskBits)
{
    LogicVector a(0b1000ULL, 0b0100ULL); // bit3='1', bit2='X'
    LogicVector r = a.lsr(2, 8);
    EXPECT_EQ(r.value, 0b10ULL);
    EXPECT_EQ(r.mask, 0b01ULL);
}

TEST(ShiftLsr, WidthZeroAlwaysShiftsOutEverything)
{
    LogicVector a = LogicVector::Ones();
    EXPECT_EQ(a.lsr(0, 0), LogicVector(0ULL, 0ULL));
}

// ============================================================================
// Shift: asr (arithmetic shift right)
// ============================================================================

TEST(ShiftAsr, PositiveMsbZeroExtendsWithZero)
{
    LogicVector a = LogicVector::FromInt(0b0100); // width 4, MSB=0
    LogicVector r = a.asr(1, 4);
    EXPECT_EQ(r.value, 0b0010ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(ShiftAsr, NegativeMsbOneSignExtendsWithOnes)
{
    LogicVector a = LogicVector::FromInt(0b1000); // width 4, MSB=1 (-8)
    LogicVector r = a.asr(1, 4);
    EXPECT_EQ(r.value, 0b1100ULL); // -4 in 4-bit two's complement
}

TEST(ShiftAsr, ShamtGreaterThanWidthFillsWithSignBit_Negative)
{
    LogicVector a = LogicVector::FromInt(0b1000); // width 4, MSB=1
    LogicVector r = a.asr(100, 4);
    EXPECT_EQ(r.value, 0b1111ULL); // fully sign-extended to all ones
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(ShiftAsr, ShamtGreaterThanWidthFillsWithSignBit_Positive)
{
    LogicVector a = LogicVector::FromInt(0b0111); // width 4, MSB=0
    LogicVector r = a.asr(100, 4);
    EXPECT_EQ(r.value, 0b0000ULL);
}

TEST(ShiftAsr, WidthZeroReturnsZero)
{
    LogicVector a = LogicVector::Ones();
    EXPECT_EQ(a.asr(1, 0), LogicVector(0ULL, 0ULL));
}

TEST(ShiftAsr, WidthAbove64IsClampedTo64)
{
    LogicVector a = LogicVector::FromInt(0x8000000000000000ULL); // MSB of 64-bit set
    LogicVector r = a.asr(4, 255); // width clamped to 64 internally
    EXPECT_EQ(r.value, 0xF800000000000000ULL);
}

TEST(ShiftAsr, DefaultWidthIs64AndSignExtendsFromBit63)
{
    LogicVector a = LogicVector::FromInt(0x8000000000000000ULL);
    LogicVector r = a.asr(4); // default width = 64
    EXPECT_EQ(r.value, 0xF800000000000000ULL);
}

TEST(ShiftAsr, PropagatesUndefinedSignBit)
{
    // MSB of a 4-bit vector is 'X' (value=0, mask=1 at bit 3).
    LogicVector a(0b0000ULL, 0b1000ULL);
    LogicVector r = a.asr(1, 4);
    // Sign-extension copies the mask bit too, so the top bits become X.
    EXPECT_EQ(r.mask, 0b1100ULL);
}

TEST(ShiftAsr, ShamtZeroIsIdentityWithinWidth)
{
    LogicVector a = LogicVector::FromInt(0b1011);
    EXPECT_EQ(a.asr(0, 4).value, 0b1011ULL);
}

// ============================================================================
// Rotate: rol (rotate left)
// ============================================================================

TEST(RotateRol, BasicRotate)
{
    LogicVector a = LogicVector::FromInt(0b0001);
    LogicVector r = a.rol(1, 4);
    EXPECT_EQ(r.value, 0b0010ULL);
}

TEST(RotateRol, WrapsAroundMsb)
{
    LogicVector a = LogicVector::FromInt(0b1000);
    LogicVector r = a.rol(1, 4);
    EXPECT_EQ(r.value, 0b0001ULL);
}

TEST(RotateRol, ShamtZeroIsIdentity)
{
    LogicVector a(0xABULL, 0x0FULL);
    LogicVector r = a.rol(0, 8);
    EXPECT_EQ(r, a);
}

TEST(RotateRol, WidthZeroReturnsZero)
{
    LogicVector a = LogicVector::Ones();
    EXPECT_EQ(a.rol(3, 0), LogicVector(0ULL, 0ULL));
}

TEST(RotateRol, ShamtEqualToWidthReturnsOriginalUnmasked)
{
    // shamt %= width makes shamt 0, and the early-return path returns *this
    // completely unmodified -- it does NOT mask to `width` in this case.
    // This documents real, possibly-surprising behavior: bits above `width`
    // survive untouched when shamt is an exact multiple of width.
    LogicVector a = LogicVector::FromInt(0xFF); // bits set beyond width=4
    LogicVector r = a.rol(8, 4);                // shamt % 4 == 0
    EXPECT_EQ(r, a) << "rol() returned *this unmasked when shamt %% width == 0";
}

TEST(RotateRol, ShamtLargerThanWidthWraps)
{
    LogicVector a = LogicVector::FromInt(0b0001);
    LogicVector r1 = a.rol(1, 4);
    LogicVector r2 = a.rol(5, 4); // 5 % 4 == 1
    EXPECT_EQ(r1, r2);
}

TEST(RotateRol, FullWidth64)
{
    LogicVector a = LogicVector::FromInt(1ULL);
    LogicVector r = a.rol(1, 64);
    EXPECT_EQ(r.value, 2ULL);

    LogicVector b = LogicVector::FromInt(0x8000000000000000ULL);
    LogicVector r2 = b.rol(1, 64);
    EXPECT_EQ(r2.value, 1ULL);
}

TEST(RotateRol, PreservesPopcountOfMaskAndValue)
{
    LogicVector a(0b1100ULL, 0b0011ULL);
    LogicVector r = a.rol(2, 4);
    EXPECT_EQ(r.value, 0b0011ULL);
    EXPECT_EQ(r.mask, 0b1100ULL);
}

// ============================================================================
// Rotate: ror (rotate right)
// ============================================================================

TEST(RotateRor, BasicRotate)
{
    LogicVector a = LogicVector::FromInt(0b0010);
    LogicVector r = a.ror(1, 4);
    EXPECT_EQ(r.value, 0b0001ULL);
}

TEST(RotateRor, WrapsAroundLsb)
{
    LogicVector a = LogicVector::FromInt(0b0001);
    LogicVector r = a.ror(1, 4);
    EXPECT_EQ(r.value, 0b1000ULL);
}

TEST(RotateRor, ShamtZeroIsIdentity)
{
    LogicVector a(0xABULL, 0x0FULL);
    EXPECT_EQ(a.ror(0, 8), a);
}

TEST(RotateRor, WidthZeroReturnsZero)
{
    LogicVector a = LogicVector::Ones();
    EXPECT_EQ(a.ror(3, 0), LogicVector(0ULL, 0ULL));
}

TEST(RotateRor, IsInverseOfRol)
{
    LogicVector a(0b10110010ULL, 0b01000001ULL);
    LogicVector rolled = a.rol(3, 8);
    LogicVector back = rolled.ror(3, 8);
    EXPECT_EQ(back, a);
}

TEST(RotateRor, FullWidth64)
{
    LogicVector a = LogicVector::FromInt(1ULL);
    LogicVector r = a.ror(1, 64);
    EXPECT_EQ(r.value, 0x8000000000000000ULL);
}

// ============================================================================
// resolve() - IEEE 1164 wired-logic bus resolution, exhaustive truth table
// ============================================================================

class ResolveTruthTable : public ::testing::TestWithParam<std::tuple<char, char, char>> {};

TEST_P(ResolveTruthTable, MatchesWiredLogicRules)
{
    auto [a, b, expected] = GetParam();
    LogicVector result = StateBit(a).resolve(StateBit(b));
    EXPECT_EQ(StateOf(result), expected) << "a=" << a << " b=" << b;
}

INSTANTIATE_TEST_SUITE_P(
    WiredLogic, ResolveTruthTable,
    ::testing::Values(
        // Equal driven bits remain unchanged.
        std::make_tuple('0', '0', '0'),
        std::make_tuple('1', '1', '1'),
        // Conflicting driven bits (bus contention) -> X.
        std::make_tuple('0', '1', 'X'),
        std::make_tuple('1', '0', 'X'),
        // X always propagates regardless of the other driver.
        std::make_tuple('X', '0', 'X'), std::make_tuple('X', '1', 'X'),
        std::make_tuple('X', 'X', 'X'), std::make_tuple('X', 'Z', 'X'),
        std::make_tuple('0', 'X', 'X'), std::make_tuple('1', 'X', 'X'),
        std::make_tuple('Z', 'X', 'X'),
        // Z yields to a driven value; Z with Z stays Z (undriven bus).
        std::make_tuple('Z', '0', '0'), std::make_tuple('Z', '1', '1'),
        std::make_tuple('0', 'Z', '0'), std::make_tuple('1', 'Z', '1'),
        std::make_tuple('Z', 'Z', 'Z')
    )
);

TEST(Resolve, IsCommutativeAtWordLevel)
{
    LogicVector a(0b1010ULL, 0b0101ULL);
    LogicVector b(0b0110ULL, 0b1001ULL);
    EXPECT_EQ(a.resolve(b), b.resolve(a));
}

TEST(Resolve, HighZResolvedWithSelfIsHighZ)
{
    LogicVector hz = LogicVector::HighZ();
    EXPECT_EQ(hz.resolve(hz), LogicVector::HighZ());
}

TEST(Resolve, DefiniteDriverWinsOverHighZ)
{
    LogicVector driver = LogicVector::FromInt(0b1010);
    LogicVector floating = LogicVector::HighZ();
    EXPECT_EQ(driver.resolve(floating), driver);
    EXPECT_EQ(floating.resolve(driver), driver);
}

TEST(Resolve, TwoDifferentDriversContend)
{
    LogicVector a = LogicVector::FromInt(0b1010);
    LogicVector b = LogicVector::FromInt(0b0110);
    LogicVector r = a.resolve(b);
    // bit0: 0 vs 0 -> 0 ; bit1: 1 vs 1 -> 1 ; bit2: 0 vs 1 -> X ; bit3: 1 vs 0 -> X
    EXPECT_EQ(r.str(4), "XX10");
}

// ============================================================================
// bit() - single-bit access
// ============================================================================

TEST(BitAccess, ReturnsCorrectCharForEachState)
{
    LogicVector lv(0b1010ULL, 0b1100ULL); // bit0='0',bit1='1',bit2='X',bit3='Z'
    EXPECT_EQ(lv.bit(0), '0');
    EXPECT_EQ(lv.bit(1), '1');
    EXPECT_EQ(lv.bit(2), 'X');
    EXPECT_EQ(lv.bit(3), 'Z');
}

TEST(BitAccess, IndexZeroIsValid)
{
    EXPECT_NO_THROW(LogicVector::Zero().bit(0));
}

TEST(BitAccess, IndexSixtyThreeIsValid)
{
    LogicVector lv = LogicVector::FromInt(0x8000000000000000ULL);
    EXPECT_EQ(lv.bit(63), '1');
}

TEST(BitAccess, IndexSixtyFourThrows)
{
    EXPECT_THROW(LogicVector::Zero().bit(64), std::out_of_range);
}

TEST(BitAccess, MaxUint8IndexThrows)
{
    EXPECT_THROW(LogicVector::Zero().bit(255), std::out_of_range);
}

// ============================================================================
// range(high, low) - bit range extraction (including reversed ranges)
// ============================================================================

TEST(RangeHighLow, StandardRangeExtractsLowBits)
{
    LogicVector lv = LogicVector::FromInt(0b11010110);
    LogicVector r = lv.range(7, 0);
    EXPECT_EQ(r.value, 0b11010110ULL);
}

TEST(RangeHighLow, PartialRangeMiddleBits)
{
    LogicVector lv = LogicVector::FromInt(0b11010110);
    LogicVector r = lv.range(5, 2); // bits [5:2] = 0101
    EXPECT_EQ(r.value, 0b0101ULL);
}

TEST(RangeHighLow, SingleBitRangeHighEqualsLow)
{
    LogicVector lv = LogicVector::FromInt(0b0100);
    LogicVector r = lv.range(2, 2);
    EXPECT_EQ(r.value, 1ULL);
    EXPECT_EQ(r.bit(0), '1');
}

TEST(RangeHighLow, FullRange63To0IsIdentity)
{
    LogicVector lv = LogicVector::FromInt(0x0123456789ABCDEFULL);
    LogicVector r = lv.range(63, 0);
    EXPECT_EQ(r, lv);
}

TEST(RangeHighLow, ReversedRangeReversesBitOrder)
{
    // bit0 = '1', everything else '0'. Reversing [0:63] (i.e. low=63,high=0
    // per the documented "getRange(0,7)" style reversal) should move that
    // single bit to the opposite end of the extracted width.
    LogicVector lv = LogicVector::FromInt(1ULL);
    LogicVector r = lv.range(0, 63); // reversed full-width range
    EXPECT_EQ(r.value, 0x8000000000000000ULL);
}

TEST(RangeHighLow, ReversedRangeSmallWidth)
{
    // bits [3:0] of 0b0001 read in reverse should become 0b1000.
    LogicVector lv = LogicVector::FromInt(0b0001);
    LogicVector r = lv.range(0, 3); // high=0 < low=3 -> reversed
    EXPECT_EQ(r.value, 0b1000ULL);
}

TEST(RangeHighLow, PropagatesMaskBits)
{
    LogicVector lv(0b0010ULL, 0b0100ULL); // bit1='1', bit2='X'
    LogicVector r = lv.range(3, 0);
    EXPECT_EQ(r.value, 0b0010ULL);
    EXPECT_EQ(r.mask, 0b0100ULL);
}

TEST(RangeHighLow, HighOutOfRangeThrows)
{
    EXPECT_THROW(LogicVector::Zero().range(64, 0), std::out_of_range);
}

TEST(RangeHighLow, LowOutOfRangeThrows)
{
    EXPECT_THROW(LogicVector::Zero().range(10, 64), std::out_of_range);
}

TEST(RangeHighLow, BothOutOfRangeThrows)
{
    EXPECT_THROW(LogicVector::Zero().range(200, 100), std::out_of_range);
}

// ============================================================================
// range(width) - prefix extraction
// ============================================================================

TEST(RangeWidth, ExtractsLowNBits)
{
    LogicVector lv = LogicVector::FromInt(0b11110101);
    LogicVector r = lv.range(4);
    EXPECT_EQ(r.value, 0b0101ULL);
}

TEST(RangeWidth, WidthZeroYieldsEmptyZero)
{
    LogicVector lv = LogicVector::Ones();
    LogicVector r = lv.range(0);
    EXPECT_EQ(r.value, 0ULL);
    EXPECT_EQ(r.mask, 0ULL);
}

TEST(RangeWidth, MaskIsAlsoTruncated)
{
    LogicVector lv(0b1111ULL, 0b1010ULL);
    LogicVector r = lv.range(2);
    EXPECT_EQ(r.value, 0b11ULL);
    EXPECT_EQ(r.mask, 0b10ULL);
}

TEST(RangeWidth, WidthAbove64Throws)
{
    EXPECT_THROW(LogicVector::Zero().range(65), std::out_of_range);
    EXPECT_THROW(LogicVector::Zero().range(255), std::out_of_range);
}

TEST(RangeWidth, FullWidth64_KnownBug)
{
    // INTENDED behavior: range(64) should be the identity (keep every bit),
    // exactly like lsl/lsr/asr/rol/ror all treat width>=64 as "use ~0ULL".
    //
    // ACTUAL behavior at the time these tests were written: range(uint8_t)
    // computes `(1ULL << width) - 1ULL` with no width>=64 guard, so
    // range(64) evaluates `1ULL << 64`, which is undefined behavior for a
    // 64-bit operand (shift amount must be < 64). On common platforms this
    // wraps to `1ULL << 0 == 1`, making wmask == 0 and silently zeroing the
    // entire vector -- the opposite of "keep everything".
    //
    // This test intentionally asserts the CORRECT/intended behavior. If it
    // fails, it is flagging the bug described above, not a mistake in the
    // test. Recommended fix in logicVector.h:
    //   uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
    LogicVector lv = LogicVector::FromInt(0x0123456789ABCDEFULL);
    LogicVector r = lv.range(64);
    EXPECT_EQ(r.value, lv.value)
        << "range(64) did not preserve all bits -- see width>=64 shift bug "
           "in LogicVector::range(uint8_t) (1ULL << 64 is UB).";
}

// ============================================================================
// isDefinite()
// ============================================================================

TEST(IsDefinite, TrueWhenMaskIsZero)
{
    EXPECT_TRUE(LogicVector::Zero().isDefinite());
    EXPECT_TRUE(LogicVector::Ones().isDefinite());
    EXPECT_TRUE(LogicVector::FromInt(12345ULL).isDefinite());
}

TEST(IsDefinite, FalseWhenAnyMaskBitSet)
{
    EXPECT_FALSE(LogicVector::Unknown().isDefinite());
    EXPECT_FALSE(LogicVector::HighZ().isDefinite());
    EXPECT_FALSE(LogicVector(0ULL, 1ULL).isDefinite());
    EXPECT_FALSE(LogicVector(0ULL, 0x8000000000000000ULL).isDefinite());
}

// ============================================================================
// str()
// ============================================================================

TEST(Str, DefaultWidthIs64)
{
    LogicVector lv = LogicVector::Zero();
    EXPECT_EQ(lv.str().size(), 64u);
}

TEST(Str, ZeroVector)
{
    EXPECT_EQ(LogicVector::Zero().str(8), "00000000");
}

TEST(Str, OnesVector)
{
    EXPECT_EQ(LogicVector::Ones().str(8), "11111111");
}

TEST(Str, UnknownVector)
{
    EXPECT_EQ(LogicVector::Unknown().str(4), "XXXX");
}

TEST(Str, HighZVector)
{
    EXPECT_EQ(LogicVector::HighZ().str(4), "ZZZZ");
}

TEST(Str, MsbFirstOrdering)
{
    LogicVector lv = LogicVector::FromInt(0b1010);
    EXPECT_EQ(lv.str(4), "1010");
}

TEST(Str, MixedStates)
{
    // bit0='0', bit1='1', bit2='X', bit3='Z' -> MSB-first: "ZX10"
    LogicVector lv(0b1010ULL, 0b1100ULL);
    EXPECT_EQ(lv.str(4), "ZX10");
}

TEST(Str, WidthZeroYieldsEmptyString)
{
    EXPECT_EQ(LogicVector::Ones().str(0), "");
}

TEST(Str, WidthGreaterThan64IsClampedTo64)
{
    LogicVector lv = LogicVector::Zero();
    EXPECT_EQ(lv.str(255).size(), 64u);
}

TEST(Str, OnlyShowsRequestedWidthIgnoringHigherBits)
{
    LogicVector lv = LogicVector::FromInt(0xFF00ULL); // bits 8-15 set
    EXPECT_EQ(lv.str(8), "00000000"); // low 8 bits are all 0
}

// ============================================================================
// Cross-cutting sanity: round trips and invariants
// ============================================================================

TEST(Invariants, EveryByteRoundTripsThroughFromIntAndUint8)
{
    for (int i = 0; i <= 0xFF; ++i)
    {
        LogicVector lv = LogicVector::FromInt(static_cast<uint64_t>(i));
        EXPECT_EQ(static_cast<uint8_t>(lv), static_cast<uint8_t>(i));
    }
}

TEST(Invariants, StrThenBitAreConsistentForAllStatesAcrossAllBitPositions)
{
    for (int pos = 0; pos < 64; ++pos)
    {
        for (char state : kStates)
        {
            LogicVector base = LogicVector::Zero();
            LogicVector one_bit = StateBit(state).lsl(static_cast<uint8_t>(pos));
            LogicVector lv(one_bit.value, one_bit.mask);
            EXPECT_EQ(lv.bit(static_cast<uint8_t>(pos)), state)
                << "state=" << state << " pos=" << pos;
        }
    }
}

TEST(Invariants, DeMorganHoldsForDefiniteOperands)
{
    LogicVector a = LogicVector::FromInt(0b10110100ULL);
    LogicVector b = LogicVector::FromInt(0b01101101ULL);
    // ~(a & b) == ~a | ~b   (only guaranteed when both operands are fully
    // definite; X/Z propagation is not boolean-algebra-clean by design).
    EXPECT_EQ(~(a & b), (~a) | (~b));
    EXPECT_EQ(~(a | b), (~a) & (~b));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}