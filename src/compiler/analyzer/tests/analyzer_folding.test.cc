// analyzer_folding.test.cc — constant folding of every operator and attribute.
//
// A folded value is observable through the static checks that use it: a constant is assigned to a subtype whose range holds only
// the expected value, so a wrong fold is an error on the accepting side and a missing one is an error on the rejecting side.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    const std::string kTypes =
        "subtype yes is boolean range true to true; subtype no is boolean range false to false; "
        "subtype one is integer range 1 to 1; subtype seven is integer range 7 to 7; subtype minus_three is integer range -3 to -3; "
        "subtype half is real range 0.5 to 0.5; subtype quarter is real range 0.25 to 0.25; subtype two_real is real range 2.0 to 2.0; "
        "subtype ten_ns is time range 10 ns to 10 ns; subtype hundred_ps is time range 100 ps to 100 ps; "
        "type color is (red, green, blue); subtype only_green is color range green to green; "
        "type word is array (7 downto 0) of std_logic; subtype bit_t is std_logic; ";

    std::string decl(const std::string& declarations)
    {
        return analysisError(inArchitecture(kTypes + declarations));
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    /// `constant c : <subtype> := <expression>;` is accepted exactly when the expression folds to the subtype's only value.
    void expectValue(const std::string& subtype, const std::string& expression, bool expected)
    {
        const std::string message = decl("constant c : " + subtype + " := " + expression + ";");
        if (expected)
            EXPECT_EQ(message, kOk) << expression << " should be in " << subtype;
        else
            EXPECT_TRUE(mentions(message, "outside the range")) << expression << " should not be in " << subtype << ": " << message;
    }
}

// ===========================================================================
// 1. INTEGERS
// ===========================================================================

TEST(Folding_Integers, Arithmetic)
{
    expectValue("seven", "3 + 4", true);
    expectValue("seven", "10 - 3", true);
    expectValue("seven", "1 * 7", true);
    expectValue("seven", "15 / 2", true);
    expectValue("seven", "22 mod 15", true);
    expectValue("seven", "27 rem 10", true);
    expectValue("seven", "7 ** 1", true);
    expectValue("one", "7 ** 0", true);
    expectValue("seven", "abs (-7)", true);
    expectValue("seven", "+7", true);
    expectValue("minus_three", "-3", true);
    expectValue("minus_three", "- (1 + 2)", true);

    expectValue("seven", "3 + 5", false);
    expectValue("seven", "15 / 3", false);
    expectValue("seven", "abs (-8)", false);
}

TEST(Folding_Integers, ModAndRemFollowTheSignOfTheirOperands)
{
    expectValue("minus_three", "(-3) rem 5", true);
    expectValue("minus_three", "(-3) mod (-5)", true);
    expectValue("seven", "(-3) mod 10", true);
    // `mod` takes the sign of its right operand, `rem` the sign of its left one.
    expectValue("minus_three", "7 mod (-10)", true);
    expectValue("seven", "7 mod (-10) + 10", true);
    expectValue("seven", "7 rem (-10)", true);
    expectValue("minus_three", "7 rem (-10) - 10", true);
    expectValue("seven", "7 mod (-10)", false);
}

TEST(Folding_Integers, ConstantsFoldThroughTheirNames)
{
    EXPECT_EQ(decl("constant a : integer := 3; constant b : integer := a + 4; constant c : seven := b;"), kOk);
    EXPECT_TRUE(mentions(decl("constant a : integer := 3; constant b : integer := a + 5; constant c : seven := b;"), "outside the range"));
}

TEST(Folding_Integers, OverflowAndDivisionByZeroAreNotFolded)
{
    // The value is unknown, so there is nothing to check it against; the design is still accepted.
    EXPECT_EQ(decl("constant a : seven := 9223372036854775807 + 1;"), kOk);
    EXPECT_EQ(decl("constant a : seven := 2 ** 200;"), kOk);
    EXPECT_EQ(decl("constant a : seven := 2 ** (-1);"), kOk);
    EXPECT_EQ(decl("constant a : seven := -9223372036854775807 - 2;"), kOk);
}

TEST(Folding_Integers, DivisionByAConstantZeroIsAnError)
{
    EXPECT_TRUE(mentions(decl("constant a : seven := 5 / 0;"), "Division by zero: the right operand of '/' is 0"));
    EXPECT_TRUE(mentions(decl("constant a : seven := 5 mod 0;"), "Division by zero: the right operand of 'mod' is 0"));
    EXPECT_TRUE(mentions(decl("constant a : seven := 5 rem 0;"), "Division by zero: the right operand of 'rem' is 0"));
    EXPECT_TRUE(mentions(decl("constant zero : integer := 0; constant a : seven := 5 / zero;"), "Division by zero"));
}

TEST(Folding_Integers, TheMostNegativeIntegerHasNoNegation)
{
    const std::string huge = "type huge is range -9223372036854775807 - 1 to 9223372036854775807; subtype huge_seven is huge range 7 to 7; "
                             "constant lowest : huge := -9223372036854775807 - 1; ";
    EXPECT_EQ(decl(huge + "constant a : huge_seven := abs lowest;"), kOk);
    EXPECT_EQ(decl(huge + "constant a : huge_seven := - lowest;"), kOk);
    EXPECT_EQ(decl(huge + "constant a : huge_seven := lowest / (-1);"), kOk);
    EXPECT_TRUE(mentions(decl(huge + "constant a : huge_seven := lowest;"), "outside the range"));
}

// ===========================================================================
// 2. COMPARISONS AND BOOLEAN LOGIC
// ===========================================================================

TEST(Folding_Boolean, IntegerComparisons)
{
    expectValue("yes", "3 = 3", true);
    expectValue("yes", "3 /= 4", true);
    expectValue("yes", "3 < 4", true);
    expectValue("yes", "3 <= 3", true);
    expectValue("yes", "4 > 3", true);
    expectValue("yes", "3 >= 3", true);

    expectValue("no", "3 = 4", true);
    expectValue("no", "3 /= 3", true);
    expectValue("no", "4 < 3", true);
    expectValue("no", "4 <= 3", true);
    expectValue("no", "3 > 4", true);
    expectValue("no", "3 >= 4", true);

    expectValue("yes", "3 = 4", false);
    expectValue("yes", "4 < 3", false);
}

TEST(Folding_Boolean, RealComparisons)
{
    expectValue("yes", "0.5 = 0.5", true);
    expectValue("yes", "0.5 /= 0.25", true);
    expectValue("yes", "0.25 < 0.5", true);
    expectValue("yes", "0.5 <= 0.5", true);
    expectValue("yes", "0.5 > 0.25", true);
    expectValue("yes", "0.5 >= 0.5", true);
    expectValue("no", "0.5 < 0.25", true);
    expectValue("no", "0.5 = 0.25", true);
    expectValue("yes", "0.5 < 0.25", false);
}

TEST(Folding_Boolean, EnumerationComparisons)
{
    expectValue("yes", "red < green", true);
    expectValue("yes", "blue > green", true);
    expectValue("yes", "green = green", true);
    expectValue("no", "blue < red", true);
    expectValue("yes", "blue < red", false);
    expectValue("only_green", "green", true);
    expectValue("only_green", "blue", false);
}

TEST(Folding_Boolean, LogicalOperators)
{
    expectValue("yes", "true and true", true);
    expectValue("no", "true and false", true);
    expectValue("yes", "false or true", true);
    expectValue("no", "false or false", true);
    expectValue("yes", "true xor false", true);
    expectValue("no", "true xor true", true);
    expectValue("yes", "true nand false", true);
    expectValue("no", "true nand true", true);
    expectValue("yes", "false nor false", true);
    expectValue("no", "false nor true", true);
    expectValue("yes", "true xnor true", true);
    expectValue("no", "true xnor false", true);

    expectValue("yes", "true and false", false);
    expectValue("no", "false or true", false);
}

TEST(Folding_Boolean, NotAndCombinations)
{
    expectValue("yes", "not false", true);
    expectValue("no", "not true", true);
    expectValue("yes", "not (3 > 4)", true);
    expectValue("yes", "(3 < 4) and (5 > 4)", true);
    expectValue("no", "(3 < 4) and (5 < 4)", true);
    expectValue("yes", "not (3 < 4) or (2 = 2)", true);
}

TEST(Folding_Boolean, ConstantsCarryTheirValueForward)
{
    EXPECT_EQ(decl("constant t : boolean := 3 < 4; constant a : yes := t; constant b : no := not t;"), kOk);
    EXPECT_TRUE(mentions(decl("constant t : boolean := 3 < 4; constant a : no := t;"), "outside the range"));
}

// ===========================================================================
// 3. REALS
// ===========================================================================

TEST(Folding_Reals, Arithmetic)
{
    expectValue("half", "0.25 + 0.25", true);
    expectValue("half", "1.0 - 0.5", true);
    expectValue("half", "0.25 * 2.0", true);
    expectValue("half", "1.0 / 2.0", true);
    expectValue("two_real", "2.0 ** 1", true);
    expectValue("quarter", "0.5 ** 2", true);
    expectValue("half", "-(-0.5)", true);
    expectValue("half", "abs (-0.5)", true);
    expectValue("half", "+0.5", true);

    expectValue("half", "0.5 + 0.5", false);
    expectValue("half", "1.0 / 4.0", false);
    expectValue("half", "abs (-0.25)", false);
}

TEST(Folding_Reals, DivisionByZeroAndUnsupportedOperatorsAreNotFolded)
{
    EXPECT_EQ(decl("constant a : half := 1.0 / 0.0;"), kOk);
}

TEST(Folding_Reals, MixedWithIntegersThroughConversion)
{
    EXPECT_EQ(decl("constant a : half := real(1) / 2.0;"), kOk);
}

// ===========================================================================
// 4. PHYSICAL VALUES
// ===========================================================================

TEST(Folding_Physical, UnitsAndMultiples)
{
    expectValue("ten_ns", "10 ns", true);
    expectValue("ten_ns", "0.01 us", true);
    expectValue("ten_ns", "10000 ps", true);
    expectValue("hundred_ps", "0.1 ns", true);
    expectValue("ten_ns", "11 ns", false);
    expectValue("ten_ns", "9 ns", false);
}

TEST(Folding_Physical, SumsAndDifferences)
{
    expectValue("ten_ns", "4 ns + 6 ns", true);
    expectValue("ten_ns", "20 ns - 10 ns", true);
    expectValue("ten_ns", "4 ns + 7 ns", false);
}

TEST(Folding_Physical, ProductsAndQuotientsWithNumbers)
{
    expectValue("ten_ns", "5 ns * 2", true);
    expectValue("ten_ns", "2 * 5 ns", true);
    expectValue("ten_ns", "20 ns / 2", true);
    expectValue("ten_ns", "5 ns * 2.0", true);
    expectValue("ten_ns", "2.0 * 5 ns", true);
    expectValue("ten_ns", "20 ns / 2.0", true);
    expectValue("ten_ns", "5 ns * 3", false);
    expectValue("ten_ns", "20 ns / 4", false);
}

TEST(Folding_Physical, TheQuotientOfTwoQuantitiesIsANumber)
{
    expectValue("seven", "70 ns / 10 ns", true);
    expectValue("seven", "70 ns / 11 ns", false);
    expectValue("seven", "1 us / 100 ns", false);
}

TEST(Folding_Physical, ComparisonsBetweenQuantities)
{
    expectValue("yes", "1 ns < 2 ns", true);
    expectValue("yes", "1 us > 999 ns", true);
    expectValue("no", "1 us < 999 ns", true);
    expectValue("yes", "1 us = 1000 ns", true);
}

TEST(Folding_Physical, OverflowIsNotFolded)
{
    EXPECT_TRUE(mentions(decl("constant a : ten_ns := 9223372036854775807 sec;"), "too large"));
    EXPECT_EQ(decl("constant a : ten_ns := 5 ns * 9223372036854775807;"), kOk);
    EXPECT_EQ(decl("constant a : ten_ns := 5 ns * 1.0e30;"), kOk);
}

// ===========================================================================
// 5. ATTRIBUTES OF ARRAYS AND SCALARS
// ===========================================================================

TEST(Folding_Attributes, ArrayBounds)
{
    EXPECT_EQ(decl("signal w : std_logic_vector(7 downto 0); constant a : seven := w'high; constant b : integer range 0 to 0 := w'low; "
                   "constant c : seven := w'left; constant d : integer range 0 to 0 := w'right; constant e : integer range 8 to 8 := w'length;"), kOk);
    EXPECT_TRUE(mentions(decl("signal w : std_logic_vector(7 downto 0); constant a : seven := w'low;"), "outside the range"));
    EXPECT_TRUE(mentions(decl("signal w : std_logic_vector(7 downto 0); constant a : seven := w'length;"), "outside the range"));
}

TEST(Folding_Attributes, AscendingArrays)
{
    EXPECT_EQ(decl("signal w : std_logic_vector(0 to 7); constant a : seven := w'right; constant b : integer range 0 to 0 := w'left; "
                   "constant c : seven := w'high; constant d : integer range 0 to 0 := w'low;"), kOk);
}

TEST(Folding_Attributes, ScalarTypeBounds)
{
    EXPECT_EQ(decl("subtype small is integer range 3 to 9; constant a : integer range 9 to 9 := small'high; constant b : integer range 3 to 3 := small'low; "
                   "constant c : integer range 3 to 3 := small'left; constant d : integer range 9 to 9 := small'right;"), kOk);
    EXPECT_TRUE(mentions(decl("subtype small is integer range 3 to 9; constant a : integer range 3 to 3 := small'high;"), "outside the range"));
}

TEST(Folding_Attributes, EnumerationBounds)
{
    EXPECT_TRUE(mentions(decl("constant a : only_green := color'high;"), "outside the range"));
    EXPECT_TRUE(mentions(decl("constant a : only_green := color'low;"), "outside the range"));
    EXPECT_EQ(decl("subtype pair is color range green to blue; constant a : only_green := pair'low;"), kOk);
}

TEST(Folding_Attributes, RealBounds)
{
    EXPECT_EQ(decl("constant a : half := half'low; constant b : half := half'high;"), kOk);
    EXPECT_EQ(decl("subtype fraction is real range 0.25 to 0.75; constant a : quarter := fraction'low; constant b : real range 0.75 to 0.75 := fraction'high;"), kOk);
    EXPECT_TRUE(mentions(decl("subtype fraction is real range 0.25 to 0.75; constant a : quarter := fraction'high;"), "outside the range"));
}

TEST(Folding_Attributes, UnknownBoundsHaveNoValue)
{
    // The length of a port whose size depends on a generic is not known, so nothing is checked against it.
    EXPECT_EQ(analysisError("entity t is generic (w : natural := 4); port (v : in std_logic_vector(w - 1 downto 0)); end t; "
                            "architecture r of t is subtype seven is integer range 7 to 7; constant a : seven := v'length; begin end r;"), kOk);
}

TEST(Folding_Attributes, ALengthOfAScalarIsNotAValue)
{
    EXPECT_TRUE(mentions(decl("subtype small is integer range 3 to 9; constant a : integer := small'length;"), "length"));
}

TEST(Folding_Attributes, SpecifiedAttributesFoldToTheirValue)
{
    EXPECT_EQ(decl("signal s : std_logic; attribute weight : integer; attribute weight of s : signal is 7; constant a : seven := s'weight;"), kOk);
    EXPECT_TRUE(mentions(decl("signal s : std_logic; attribute weight : integer; attribute weight of s : signal is 8; constant a : seven := s'weight;"),
                         "outside the range"));
}

// ===========================================================================
// 6. WHAT MUST BE STATIC
// ===========================================================================

TEST(Folding_Static, TypeRangesNeedConstantBounds)
{
    EXPECT_TRUE(mentions(decl("signal n : integer; type t is range 0 to n;"), "constant"));
    EXPECT_TRUE(mentions(decl("signal n : integer; subtype t is integer range 0 to n;"), "constant"));
    EXPECT_TRUE(mentions(decl("signal n : integer; signal v : std_logic_vector(n downto 0);"), "constant"));
}

TEST(Folding_Static, TypeRangesNeedNonEmptyIntegerBounds)
{
    EXPECT_TRUE(mentions(decl("type t is range 5 to 1;"), "cannot be empty"));
    EXPECT_TRUE(mentions(decl("type t is range 1 downto 5;"), "cannot be empty"));
    EXPECT_TRUE(mentions(decl("type t is range 0.5 to 1.5 units u; end units;"), "must use integer bounds"));
}
