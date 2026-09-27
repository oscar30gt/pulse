// checked_math.test.cc — overflow-checked 64-bit arithmetic, at the boundaries and for every combination of signs.
//
// The checks must hold in optimized builds too: a check made on an overflowed result is undefined behavior that an
// optimizer may remove, which is why they are made before the operation.

#include <gtest/gtest.h>

#include "checked_math.h"

using namespace Pulse;

TEST(CheckedMath, MultiplicationInRange)
{
    EXPECT_EQ(checkedMul(0, kInt64Min), 0);
    EXPECT_EQ(checkedMul(6, 7), 42);
    EXPECT_EQ(checkedMul(-6, 7), -42);
    EXPECT_EQ(checkedMul(6, -7), -42);
    EXPECT_EQ(checkedMul(-6, -7), 42);
    EXPECT_EQ(checkedMul(kInt64Max, 1), kInt64Max);
    EXPECT_EQ(checkedMul(kInt64Min, 1), kInt64Min);
    EXPECT_EQ(checkedMul(kInt64Max, -1), -kInt64Max);
    EXPECT_EQ(checkedMul(int64_t{ 1 } << 62, -2), kInt64Min);
    EXPECT_EQ(checkedMul(3'037'000'499, 3'037'000'499), int64_t{ 9'223'372'030'926'249'001 });
}

TEST(CheckedMath, MultiplicationOverflows)
{
    EXPECT_EQ(checkedMul(kInt64Max, 2), std::nullopt);             // + * +
    EXPECT_EQ(checkedMul(kInt64Max, -2), std::nullopt);            // + * -
    EXPECT_EQ(checkedMul(kInt64Min, 2), std::nullopt);             // - * +
    EXPECT_EQ(checkedMul(kInt64Min, -1), std::nullopt);            // - * -
    EXPECT_EQ(checkedMul(-1, kInt64Min), std::nullopt);
    EXPECT_EQ(checkedMul(int64_t{ 1 } << 62, 2), std::nullopt);
    EXPECT_EQ(checkedMul(3'037'000'500, 3'037'000'500), std::nullopt);
    EXPECT_EQ(checkedMul(5'000'000, kInt64Max), std::nullopt);
}

TEST(CheckedMath, PowersAndSums)
{
    EXPECT_EQ(checkedPower(2, 62), int64_t{ 1 } << 62);
    EXPECT_EQ(checkedPower(2, 63), std::nullopt);
    EXPECT_EQ(checkedPower(2, 200), std::nullopt);
    EXPECT_EQ(checkedPower(-2, 63), kInt64Min);
    EXPECT_EQ(checkedAdd(kInt64Max, 1), std::nullopt);
    EXPECT_EQ(checkedSub(kInt64Min, 1), std::nullopt);
    EXPECT_EQ(checkedAdd(kInt64Max, -1), kInt64Max - 1);
}
