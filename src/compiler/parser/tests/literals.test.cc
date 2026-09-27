// literals.test.cc — numbers, physical literals, characters, strings and bit strings.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    /// The value parsed from `y <= <literal>;`.
    ExpressionPtr literal(const std::string& text)
    {
        ASTRoot root = parseSource(inArchitecture("", "y <= " + text + ";"));
        return std::move(dynamic_cast<SignalAssignment&>(*firstArchitecture(root)->body.at(0)).value);
    }

    int64_t integer(const std::string& text)
    {
        ExpressionPtr value = literal(text);
        auto* number = dynamic_cast<const IntegerLiteralExpr*>(value.get());
        EXPECT_NE(number, nullptr) << text << " is not an integer literal";
        return number ? number->value : -1;
    }

    double real(const std::string& text)
    {
        ExpressionPtr value = literal(text);
        auto* number = dynamic_cast<const DoubleLiteralExpr*>(value.get());
        EXPECT_NE(number, nullptr) << text << " is not a real literal";
        return number ? number->value : -1.0;
    }

    std::string string(const std::string& text)
    {
        ExpressionPtr value = literal(text);
        auto* characters = dynamic_cast<const StringLiteralExpr*>(value.get());
        EXPECT_NE(characters, nullptr) << text << " is not a string literal";
        return characters ? characters->value : "<none>";
    }

    std::string errorOf(const std::string& text)
    {
        return syntaxError(inArchitecture("", "y <= " + text + ";")).message;
    }
}

// ===========================================================================
// 1. NUMBERS
// ===========================================================================

TEST(ParserLiterals, DecimalIntegers)
{
    EXPECT_EQ(integer("0"), 0);
    EXPECT_EQ(integer("42"), 42);
    EXPECT_EQ(integer("1_000_000"), 1000000);
    EXPECT_EQ(integer("12e3"), 12000);
    EXPECT_EQ(integer("1E+2"), 100);
    EXPECT_EQ(integer("0e99999"), 0) << "zero stays zero whatever the exponent";
}

TEST(ParserLiterals, BasedIntegers)
{
    EXPECT_EQ(integer("16#FF#"), 255);
    EXPECT_EQ(integer("2#1010_1010#"), 170);
    EXPECT_EQ(integer("8#777#"), 511);
    EXPECT_EQ(integer("16#1#e2"), 256);
    EXPECT_EQ(integer("16#7fff_ffff_ffff_ffff#"), INT64_MAX);
}

TEST(ParserLiterals, IntegerOverflowIsASyntaxError)
{
    EXPECT_NE(errorOf("9223372036854775808").find("too large"), std::string::npos);
    EXPECT_NE(errorOf("16#1_0000_0000_0000_0000#").find("too large"), std::string::npos);
    EXPECT_NE(errorOf("1e19").find("too large"), std::string::npos);
    EXPECT_EQ(integer("9223372036854775807"), INT64_MAX);
}

TEST(ParserLiterals, Reals)
{
    EXPECT_DOUBLE_EQ(real("3.25"), 3.25);
    EXPECT_DOUBLE_EQ(real("1.5e-3"), 0.0015);
    EXPECT_DOUBLE_EQ(real("1_0.0_5"), 10.05);
    EXPECT_DOUBLE_EQ(real("2#1.1#"), 1.5);
    EXPECT_DOUBLE_EQ(real("16#F.8#e1"), 248.0);
    EXPECT_DOUBLE_EQ(real("2#1.0#e-2"), 0.25);
}

TEST(ParserLiterals, RealOverflowIsASyntaxError)
{
    EXPECT_NE(errorOf("1.0e999").find("too large"), std::string::npos);
}

TEST(ParserLiterals, PhysicalLiterals)
{
    ExpressionPtr value = literal("10 ns");
    auto* physical = as<PhysicalLiteralExpr>(value.get());
    EXPECT_EQ(physical->unit, "ns");
    EXPECT_EQ(render(physical->magnitude.get()), "10");

    ExpressionPtr fractional = literal("1.5 US");
    EXPECT_EQ(as<PhysicalLiteralExpr>(fractional.get())->unit, "us");
    EXPECT_NE(dynamic_cast<const DoubleLiteralExpr*>(as<PhysicalLiteralExpr>(fractional.get())->magnitude.get()), nullptr);
}

// ===========================================================================
// 2. CHARACTERS AND STRINGS
// ===========================================================================

TEST(ParserLiterals, CharactersKeepTheirCase)
{
    EXPECT_EQ(render(literal("'Z'").get()), "'Z'");
    EXPECT_EQ(render(literal("'z'").get()), "'z'");
    EXPECT_EQ(render(literal("'''").get()), "'''");
}

TEST(ParserLiterals, PlainStrings)
{
    EXPECT_EQ(string("\"Hello World\""), "Hello World");
    EXPECT_EQ(string("\"\""), "");
    EXPECT_EQ(string("\"say \"\"hi\"\"\""), "say \"hi\"");
    EXPECT_EQ(string("\"0101\""), "0101");
    EXPECT_EQ(string("\"1100_0011\""), "1100_0011") << "an underscore is a character of a plain string";
}

TEST(ParserLiterals, BinaryOctalHexBitStrings)
{
    EXPECT_EQ(string("b\"1010\""), "1010");
    EXPECT_EQ(string("B\"1_0_1\""), "101");
    EXPECT_EQ(string("o\"17\""), "001111");
    EXPECT_EQ(string("x\"A5\""), "10100101");
    EXPECT_EQ(string("X\"a5\""), "10100101");
    EXPECT_EQ(string("x\"\""), "");
}

TEST(ParserLiterals, ExtendedDigitsAreRepeatedAsWritten)
{
    EXPECT_EQ(string("x\"Z\""), "ZZZZ");
    EXPECT_EQ(string("x\"1-\""), "0001----");
    EXPECT_EQ(string("o\"x\""), "xxx");
    EXPECT_EQ(string("b\"UX01\""), "UX01");
}

TEST(ParserLiterals, SizedBitStrings)
{
    EXPECT_EQ(string("8x\"F\""), "00001111") << "unsized sign: zero fill";
    EXPECT_EQ(string("8ux\"F\""), "00001111");
    EXPECT_EQ(string("8sx\"F\""), "11111111") << "signed: sign extension";
    EXPECT_EQ(string("6sb\"01\""), "000001");
    EXPECT_EQ(string("3x\"7\""), "111") << "dropping leading zeros is lossless";
    EXPECT_EQ(string("3sx\"F\""), "111") << "dropping copies of the sign bit is lossless";
    EXPECT_EQ(string("4x\"Z\""), "ZZZZ");
    EXPECT_EQ(string("4x\"\""), "0000");
}

TEST(ParserLiterals, SizedBitStringsThatDoNotFit)
{
    EXPECT_NE(errorOf("3x\"F\"").find("does not fit in 3"), std::string::npos);
    EXPECT_NE(errorOf("3sx\"7\"").find("does not fit in 3"), std::string::npos);
    EXPECT_NE(errorOf("99999999b\"1\"").find("too wide"), std::string::npos);
}

TEST(ParserLiterals, DecimalBitStrings)
{
    EXPECT_EQ(string("d\"0\""), "0");
    EXPECT_EQ(string("d\"10\""), "1010");
    EXPECT_EQ(string("8d\"255\""), "11111111");
    EXPECT_EQ(string("12d\"5\""), "000000000101");
    EXPECT_EQ(string("d\"18446744073709551616\""), "1" + std::string(64, '0')) << "no 64-bit limit";
    EXPECT_NE(errorOf("4d\"16\"").find("does not fit"), std::string::npos);
}

TEST(ParserLiterals, StringLocations)
{
    ExpressionPtr value = literal("x\"FF\"");
    EXPECT_EQ(value->source.line, 5u);
    EXPECT_EQ(value->source.column, 6u);
}
