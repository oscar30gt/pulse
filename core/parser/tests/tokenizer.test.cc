// tokenizer.test.cc — GTest suite for Pulse::Parser::Tokenizer
//
// Each test section targets a specific lexical construct.
// The tokenizer is case-insensitive: all keywords / identifiers are
// normalised to lower-case before classification.

#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#include "tokenizer.h"

using namespace Pulse::Parser;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a Tokenizer from a raw string and collect every token into a vector.
static std::vector<Token> tokenize(const std::string& source)
{
    std::istringstream ss(source);
    Tokenizer tok(ss);

    std::vector<Token> result;
    result.reserve(tok.size());

    Token t;
    while (tok >> t)
        result.push_back(t);

    return result;
}

/// Shorthand: assert that tokens[idx] matches the expected type and value.
static void expectToken(const std::vector<Token>& tokens,
                        size_t                    idx,
                        TokenType                 expectedType,
                        const std::string&        expectedValue)
{
    ASSERT_LT(idx, tokens.size())
        << "Token index " << idx << " out of range (total=" << tokens.size() << ")";
    EXPECT_EQ(tokens[idx].type,  expectedType)  << "at token[" << idx << "]";
    EXPECT_EQ(tokens[idx].value, expectedValue) << "at token[" << idx << "]";
}

// ===========================================================================
// 1. KEYWORDS
// ===========================================================================

TEST(Tokenizer_Keywords, BasicKeywordsRecognised)
{
    // One keyword per line – exercise every entry in the keyword map.
    const std::vector<std::string> kws = {
        "entity", "architecture", "is", "begin", "end",
        "port", "map", "signal", "constant", "generic",
        "process", "if", "then", "else", "elsif",
        "case", "when", "with", "select", "for",
        "in", "out", "downto", "to", "open",
        "component", "use", "library", "others", "all"
    };

    for (const auto& kw : kws)
    {
        auto tokens = tokenize(kw);
        ASSERT_EQ(tokens.size(), 1u) << "keyword: " << kw;
        EXPECT_EQ(tokens[0].type,  TokenType::Keyword) << "keyword: " << kw;
        EXPECT_EQ(tokens[0].value, kw)                 << "keyword: " << kw;
    }
}

TEST(Tokenizer_Keywords, KeywordsAreCaseInsensitive)
{
    // The tokenizer lower-cases before classifying.
    auto tokens = tokenize("ENTITY Architecture IS");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type,  TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "entity");
    EXPECT_EQ(tokens[1].type,  TokenType::Keyword);
    EXPECT_EQ(tokens[1].value, "architecture");
    EXPECT_EQ(tokens[2].type,  TokenType::Keyword);
    EXPECT_EQ(tokens[2].value, "is");
}

// ===========================================================================
// 2. STANDARD TYPES
// ===========================================================================

TEST(Tokenizer_StandardTypes, AllStandardTypesRecognised)
{
    const std::vector<std::string> types = {
        "std_logic", "std_logic_vector",
        "integer", "real", "boolean",
        "character", "string",
        "signed", "unsigned"
    };

    for (const auto& ty : types)
    {
        auto tokens = tokenize(ty);
        ASSERT_EQ(tokens.size(), 1u) << "type: " << ty;
        EXPECT_EQ(tokens[0].type,  TokenType::StandardType) << "type: " << ty;
        EXPECT_EQ(tokens[0].value, ty)                      << "type: " << ty;
    }
}

TEST(Tokenizer_StandardTypes, CaseInsensitive)
{
    auto tokens = tokenize("STD_LOGIC STD_LOGIC_VECTOR INTEGER");
    ASSERT_EQ(tokens.size(), 3u);
    for (const auto& t : tokens)
        EXPECT_EQ(t.type, TokenType::StandardType);
}

// ===========================================================================
// 3. STANDARD FUNCTIONS
// ===========================================================================

TEST(Tokenizer_StandardFunctions, RecognisedFunctions)
{
    const std::vector<std::string> fns = {
        "rising_edge", "falling_edge",
        "to_unsigned", "to_signed"
    };

    for (const auto& fn : fns)
    {
        auto tokens = tokenize(fn);
        ASSERT_EQ(tokens.size(), 1u) << "function: " << fn;
        EXPECT_EQ(tokens[0].type,  TokenType::StandardFunction) << "function: " << fn;
        EXPECT_EQ(tokens[0].value, fn)                          << "function: " << fn;
    }
}

// ===========================================================================
// 4. ATTRIBUTES
// ===========================================================================

TEST(Tokenizer_Attributes, RecognisedAttributes)
{
    const std::vector<std::string> attrs = {
        "left", "right", "low", "high", "length"
    };

    for (const auto& a : attrs)
    {
        auto tokens = tokenize(a);
        ASSERT_EQ(tokens.size(), 1u) << "attribute: " << a;
        EXPECT_EQ(tokens[0].type,  TokenType::Attribute) << "attribute: " << a;
        EXPECT_EQ(tokens[0].value, a)                    << "attribute: " << a;
    }
}

TEST(Tokenizer_Attributes, AttributeAccessWithTick)
{
    // sig'length  ->  Identifier  Operator(')  Attribute
    auto tokens = tokenize("sig'length");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "sig");
    EXPECT_EQ(tokens[1].type,  TokenType::Operator);
    EXPECT_EQ(tokens[1].value, "'");
    EXPECT_EQ(tokens[2].type,  TokenType::Attribute);
    EXPECT_EQ(tokens[2].value, "length");
}

// ===========================================================================
// 5. WORD OPERATORS  (logical / shift)
// ===========================================================================

TEST(Tokenizer_WordOperators, LogicalAndShiftOperators)
{
    const std::vector<std::string> ops = {
        "and", "or", "nand", "nor",
        "xor", "xnor", "not",
        "sll", "srl", "sra", "rol", "ror"
    };

    for (const auto& op : ops)
    {
        auto tokens = tokenize(op);
        ASSERT_EQ(tokens.size(), 1u) << "word-op: " << op;
        EXPECT_EQ(tokens[0].type,  TokenType::Operator) << "word-op: " << op;
        EXPECT_EQ(tokens[0].value, op)                  << "word-op: " << op;
    }
}

// ===========================================================================
// 6. SYMBOLIC TWO-CHARACTER OPERATORS
// ===========================================================================

TEST(Tokenizer_TwoCharOperators, AllTwoCharOperators)
{
    // := <= >= => /= **
    const std::vector<std::string> ops = {":=", "<=", ">=", "=>", "/=", "**"};

    for (const auto& op : ops)
    {
        auto tokens = tokenize(op);
        ASSERT_EQ(tokens.size(), 1u) << "two-char-op: " << op;
        EXPECT_EQ(tokens[0].type,  TokenType::Operator) << "two-char-op: " << op;
        EXPECT_EQ(tokens[0].value, op)                  << "two-char-op: " << op;
    }
}

TEST(Tokenizer_TwoCharOperators, TwoCharHasPriorityOverSingleChar)
{
    // ":=" must NOT produce two tokens (':' then '=').
    auto tokens = tokenize(":=");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].value, ":=");
}

// ===========================================================================
// 7. SINGLE-CHARACTER OPERATORS
// ===========================================================================

TEST(Tokenizer_SingleCharOperators, AllSingleCharOperators)
{
    // & + - * / < > = |
    const std::vector<char> ops = {'&', '+', '-', '*', '/', '<', '>', '=', '|'};

    for (char op : ops)
    {
        std::string src(1, op);
        auto tokens = tokenize(src);
        ASSERT_EQ(tokens.size(), 1u) << "single-char-op: " << op;
        EXPECT_EQ(tokens[0].type,  TokenType::Operator) << "single-char-op: " << op;
        EXPECT_EQ(tokens[0].value, src)                 << "single-char-op: " << op;
    }
}

// ===========================================================================
// 8. DELIMITERS
// ===========================================================================

TEST(Tokenizer_Delimiters, AllDelimitersRecognised)
{
    // ( ) [ ] , ; : .
    const std::vector<char> delims = {'(', ')', '[', ']', ',', ';', ':', '.'};

    for (char d : delims)
    {
        std::string src(1, d);
        auto tokens = tokenize(src);
        ASSERT_EQ(tokens.size(), 1u) << "delimiter: " << d;
        EXPECT_EQ(tokens[0].type,  TokenType::Delimiter) << "delimiter: " << d;
        EXPECT_EQ(tokens[0].value, src)                  << "delimiter: " << d;
    }
}

// ===========================================================================
// 9. IDENTIFIERS
// ===========================================================================

TEST(Tokenizer_Identifiers, SimpleIdentifier)
{
    auto tokens = tokenize("my_signal");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "my_signal");
}

TEST(Tokenizer_Identifiers, IdentifierLowercased)
{
    // Non-keyword identifiers are also stored lower-case.
    auto tokens = tokenize("MyEntity");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "myentity");
}

TEST(Tokenizer_Identifiers, IdentifierWithDigits)
{
    auto tokens = tokenize("sig_2");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "sig_2");
}

TEST(Tokenizer_Identifiers, IdentifierStartsWithUnderscore)
{
    // '_' is a valid start character for identifiers in this tokenizer.
    auto tokens = tokenize("_hidden");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "_hidden");
}

// ===========================================================================
// 10. NUMERIC LITERALS
// ===========================================================================

TEST(Tokenizer_NumericLiterals, IntegerLiteral)
{
    auto tokens = tokenize("42");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "42");
}

TEST(Tokenizer_NumericLiterals, RealLiteral)
{
    auto tokens = tokenize("3.14");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "3.14");
}

TEST(Tokenizer_NumericLiterals, UnderscoreSeparator)
{
    // VHDL allows underscores in numeric literals for readability.
    auto tokens = tokenize("1_000_000");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "1_000_000");
}

TEST(Tokenizer_NumericLiterals, BasedLiteral)
{
    // 16#FF# is a VHDL based literal.
    auto tokens = tokenize("16#FF#");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "16#ff#"); // lower-cased
}

TEST(Tokenizer_NumericLiterals, ExponentNotation)
{
    auto tokens = tokenize("1.5E+3");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "1.5e+3");
}

// ===========================================================================
// 11. BIT-STRING LITERALS
// ===========================================================================

TEST(Tokenizer_BitStringLiterals, HexBitString)
{
    // x"A_5" — identifier-prefix form
    auto tokens = tokenize("x\"A_5\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "x\"A_5\"");
}

TEST(Tokenizer_BitStringLiterals, BinaryBitString)
{
    auto tokens = tokenize("b\"1010\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "b\"1010\"");
}

TEST(Tokenizer_BitStringLiterals, OctalBitString)
{
    auto tokens = tokenize("o\"37\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "o\"37\"");
}

TEST(Tokenizer_BitStringLiterals, UnsignedBitStringPrefixes)
{
    // ub, uo, ux
    for (const std::string& prefix : {"ub", "uo", "ux"})
    {
        std::string src = prefix + "\"10\"";
        auto tokens = tokenize(src);
        ASSERT_EQ(tokens.size(), 1u) << "prefix: " << prefix;
        EXPECT_EQ(tokens[0].type, TokenType::BitStringLiteral) << "prefix: " << prefix;
    }
}

TEST(Tokenizer_BitStringLiterals, SignedBitStringPrefixes)
{
    // sb, so, sx
    for (const std::string& prefix : {"sb", "so", "sx"})
    {
        std::string src = prefix + "\"10\"";
        auto tokens = tokenize(src);
        ASSERT_EQ(tokens.size(), 1u) << "prefix: " << prefix;
        EXPECT_EQ(tokens[0].type, TokenType::BitStringLiteral) << "prefix: " << prefix;
    }
}

TEST(Tokenizer_BitStringLiterals, SizedHexBitString)
{
    // 8x"FF" — numeric-size prefix form
    auto tokens = tokenize("8x\"FF\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "8x\"FF\"");
}

TEST(Tokenizer_BitStringLiterals, UnterminatedThrows)
{
    EXPECT_THROW(tokenize("x\"1010"), std::runtime_error);
}

// ===========================================================================
// 12. STRING LITERALS
// ===========================================================================

TEST(Tokenizer_StringLiterals, SimpleStringLiteral)
{
    auto tokens = tokenize("\"hello\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "\"hello\"");
}

TEST(Tokenizer_StringLiterals, EmptyString)
{
    auto tokens = tokenize("\"\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "\"\"");
}

TEST(Tokenizer_StringLiterals, UnterminatedStringThrows)
{
    EXPECT_THROW(tokenize("\"unterminated"), std::runtime_error);
}

TEST(Tokenizer_StringLiterals, MultilineStringThrows)
{
    // A string that spans a line break is illegal.
    EXPECT_THROW(tokenize("\"line1\nline2\""), std::runtime_error);
}

// ===========================================================================
// 13. CHARACTER LITERALS
// ===========================================================================

TEST(Tokenizer_CharacterLiterals, SingleCharLiteral)
{
    auto tokens = tokenize("'A'");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::CharacterLiteral);
    EXPECT_EQ(tokens[0].value, "'A'");
}

TEST(Tokenizer_CharacterLiterals, StdLogicValues)
{
    for (char c : {'0', '1', 'X', 'Z', 'U'})
    {
        std::string src = {'\'', c, '\''};
        auto tokens = tokenize(src);
        ASSERT_EQ(tokens.size(), 1u) << "char: " << c;
        EXPECT_EQ(tokens[0].type,  TokenType::CharacterLiteral) << "char: " << c;
        EXPECT_EQ(tokens[0].value, src)                         << "char: " << c;
    }
}

TEST(Tokenizer_CharacterLiterals, TickWithoutCharIsOperator)
{
    // A lone tick that is not 'X' form -> Operator token.
    // e.g.  vec'(others => '0')  starts with  Identifier tick ...
    auto tokens = tokenize("vec'");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[1].type,  TokenType::Operator);
    EXPECT_EQ(tokens[1].value, "'");
}

// ===========================================================================
// 14. COMMENTS
// ===========================================================================

TEST(Tokenizer_Comments, LineCommentSkipped)
{
    // Everything after -- to end-of-line must be ignored.
    auto tokens = tokenize("signal -- this is ignored\n");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "signal");
}

TEST(Tokenizer_Comments, CommentAfterTokens)
{
    auto tokens = tokenize("end -- EOF");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].value, "end");
}

TEST(Tokenizer_Comments, CommentOnlySource)
{
    // A source with only comments should produce no tokens.
    auto tokens = tokenize("-- nothing here\n-- or here\n");
    EXPECT_EQ(tokens.size(), 0u);
}

TEST(Tokenizer_Comments, InlineCommentDoesNotEatNextLine)
{
    auto tokens = tokenize("signal -- comment\nmy_sig");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].value, "signal");
    EXPECT_EQ(tokens[1].value, "my_sig");
}

TEST(Tokenizer_Comments, CommentAttachedToEndKeyword)
{
    // "end--comment" — the '--' immediately follows a keyword with no space.
    auto tokens = tokenize("end--comment\n");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "end");
}

// ===========================================================================
// 15. WHITESPACE
// ===========================================================================

TEST(Tokenizer_Whitespace, LeadingAndTrailingWhitespaceIgnored)
{
    auto tokens = tokenize("  \t  signal  \t  ");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].value, "signal");
}

TEST(Tokenizer_Whitespace, NewlinesTreatedAsWhitespace)
{
    auto tokens = tokenize("signal\n\nmy_sig");
    ASSERT_EQ(tokens.size(), 2u);
}

// ===========================================================================
// 16. POSITION TRACKING (line / column)
// ===========================================================================

TEST(Tokenizer_PositionTracking, FirstTokenOnFirstLine)
{
    auto tokens = tokenize("entity");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].line,   1u);
    EXPECT_EQ(tokens[0].column, 1u);
}

TEST(Tokenizer_PositionTracking, SecondTokenOnSameLine)
{
    auto tokens = tokenize("entity foo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[1].line,   1u);
    EXPECT_EQ(tokens[1].column, 8u); // "entity " = 7 chars, then 'f' at col 8
}

TEST(Tokenizer_PositionTracking, TokenOnSecondLine)
{
    auto tokens = tokenize("entity\nfoo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[1].line,   2u);
    EXPECT_EQ(tokens[1].column, 1u);
}

// ===========================================================================
// 17. UNKNOWN CHARACTER THROWS
// ===========================================================================

TEST(Tokenizer_UnknownChar, ThrowsOnUnrecognisedCharacter)
{
    // '@' is not in any recognised category.
    EXPECT_THROW(tokenize("@"), std::runtime_error);
}

TEST(Tokenizer_UnknownChar, ThrowsOnBacktick)
{
    EXPECT_THROW(tokenize("`"), std::runtime_error);
}

// ===========================================================================
// 18. COMPOUND / INTEGRATION TESTS
// ===========================================================================

TEST(Tokenizer_Integration, EntityDeclaration)
{
    // entity adder is port ( a : in std_logic ; b : in std_logic ) ; end adder ;
    const std::string src =
        "entity adder is\n"
        "  port ( a : in std_logic;\n"
        "         b : in std_logic);\n"
        "end adder;\n";

    auto tokens = tokenize(src);

    expectToken(tokens, 0,  TokenType::Keyword,      "entity");
    expectToken(tokens, 1,  TokenType::Identifier,   "adder");
    expectToken(tokens, 2,  TokenType::Keyword,       "is");
    expectToken(tokens, 3,  TokenType::Keyword,       "port");
    expectToken(tokens, 4,  TokenType::Delimiter,     "(");
    expectToken(tokens, 5,  TokenType::Identifier,    "a");
    expectToken(tokens, 6,  TokenType::Delimiter,     ":");
    expectToken(tokens, 7,  TokenType::Keyword,       "in");
    expectToken(tokens, 8,  TokenType::StandardType,  "std_logic");
    expectToken(tokens, 9,  TokenType::Delimiter,     ";");
    expectToken(tokens, 10, TokenType::Identifier,    "b");
    expectToken(tokens, 11, TokenType::Delimiter,     ":");
    expectToken(tokens, 12, TokenType::Keyword,       "in");
    expectToken(tokens, 13, TokenType::StandardType,  "std_logic");
    expectToken(tokens, 14, TokenType::Delimiter,     ")");
    expectToken(tokens, 15, TokenType::Delimiter,     ";");
    expectToken(tokens, 16, TokenType::Keyword,       "end");
    expectToken(tokens, 17, TokenType::Identifier,    "adder");
    expectToken(tokens, 18, TokenType::Delimiter,     ";");
    EXPECT_EQ(tokens.size(), 19u);
}

TEST(Tokenizer_Integration, SignalAssignment)
{
    // y <= a and b;
    auto tokens = tokenize("y <= a and b;");
    ASSERT_EQ(tokens.size(), 6u);
    expectToken(tokens, 0, TokenType::Identifier, "y");
    expectToken(tokens, 1, TokenType::Operator,   "<=");
    expectToken(tokens, 2, TokenType::Identifier, "a");
    expectToken(tokens, 3, TokenType::Operator,   "and");
    expectToken(tokens, 4, TokenType::Identifier, "b");
    expectToken(tokens, 5, TokenType::Delimiter,  ";");
}

TEST(Tokenizer_Integration, VariableAssignmentWithConstant)
{
    // count := count + 1;
    auto tokens = tokenize("count := count + 1;");
    ASSERT_EQ(tokens.size(), 6u);
    expectToken(tokens, 0, TokenType::Identifier,    "count");
    expectToken(tokens, 1, TokenType::Operator,      ":=");
    expectToken(tokens, 2, TokenType::Identifier,    "count");
    expectToken(tokens, 3, TokenType::Operator,      "+");
    expectToken(tokens, 4, TokenType::NumericLiteral, "1");
    expectToken(tokens, 5, TokenType::Delimiter,     ";");
}

TEST(Tokenizer_Integration, IfThenElse)
{
    const std::string src =
        "if sel = '1' then\n"
        "  y <= a;\n"
        "else\n"
        "  y <= b;\n"
        "end if;\n";

    auto tokens = tokenize(src);

    expectToken(tokens, 0,  TokenType::Keyword,          "if");
    expectToken(tokens, 1,  TokenType::Identifier,        "sel");
    expectToken(tokens, 2,  TokenType::Operator,          "=");
    expectToken(tokens, 3,  TokenType::CharacterLiteral,  "'1'");
    expectToken(tokens, 4,  TokenType::Keyword,            "then");
    expectToken(tokens, 5,  TokenType::Identifier,         "y");
    expectToken(tokens, 6,  TokenType::Operator,           "<=");
    expectToken(tokens, 7,  TokenType::Identifier,         "a");
    expectToken(tokens, 8,  TokenType::Delimiter,          ";");
    expectToken(tokens, 9,  TokenType::Keyword,            "else");
    expectToken(tokens, 10, TokenType::Identifier,         "y");
    expectToken(tokens, 11, TokenType::Operator,           "<=");
    expectToken(tokens, 12, TokenType::Identifier,         "b");
    expectToken(tokens, 13, TokenType::Delimiter,          ";");
    expectToken(tokens, 14, TokenType::Keyword,            "end");
    expectToken(tokens, 15, TokenType::Keyword,            "if");
    expectToken(tokens, 16, TokenType::Delimiter,          ";");
    EXPECT_EQ(tokens.size(), 17u);
}

TEST(Tokenizer_Integration, ProcessWithRisingEdge)
{
    const std::string src =
        "process(clk)\n"
        "begin\n"
        "  if rising_edge(clk) then\n"
        "    q <= d;\n"
        "  end if;\n"
        "end process;\n";

    auto tokens = tokenize(src);

    expectToken(tokens, 0,  TokenType::Keyword,          "process");
    expectToken(tokens, 1,  TokenType::Delimiter,         "(");
    expectToken(tokens, 2,  TokenType::Identifier,        "clk");
    expectToken(tokens, 3,  TokenType::Delimiter,         ")");
    expectToken(tokens, 4,  TokenType::Keyword,            "begin");
    expectToken(tokens, 5,  TokenType::Keyword,            "if");
    expectToken(tokens, 6,  TokenType::StandardFunction,   "rising_edge");
    expectToken(tokens, 7,  TokenType::Delimiter,          "(");
    expectToken(tokens, 8,  TokenType::Identifier,         "clk");
    expectToken(tokens, 9,  TokenType::Delimiter,          ")");
    expectToken(tokens, 10, TokenType::Keyword,            "then");
}

TEST(Tokenizer_Integration, CompressedSyntaxFile)
{
    // Verifies the tokenizer handles the compressed style found in
    // test-files/compressedSyntax.vhdl without throwing.
    const std::string src =
        "library IEEE;use IEEE.STD_LOGIC_1164.ALL;\n"
        "entity test_mux is port(a,b:in std_logic_vector(3 downto 0);\n"
        "sel:in std_logic;y:out std_logic_vector(3 downto 0));\n"
        "end test_mux;--hello world\n"
        "architecture Behavior of test_mux is\n"
        "signal temp:std_logic_vector(3 downto 0):=\"0000\";\n"
        "begin\n"
        "process(a,b,sel)begin\n"
        "if(sel='1')then y<=a;elsif(sel='0')then y<=b;else\n"
        "y<=\"1111\";end if;temp<=x\"A_5\";\n"
        "if rising_edge(sel) then\n"
        "temp<=x\"F_0\";end if;\n"
        "end process;end Behavior;\n";

    EXPECT_NO_THROW({
        auto tokens = tokenize(src);
        EXPECT_GT(tokens.size(), 0u);
    });
}

TEST(Tokenizer_Integration, UseLibraryClause)
{
    // library IEEE ; use IEEE . STD_LOGIC_1164 . ALL ;
    auto tokens = tokenize("library IEEE;\nuse IEEE.STD_LOGIC_1164.ALL;");

    expectToken(tokens, 0, TokenType::Keyword,    "library");
    expectToken(tokens, 1, TokenType::Identifier,  "ieee");
    expectToken(tokens, 2, TokenType::Delimiter,   ";");
    expectToken(tokens, 3, TokenType::Keyword,     "use");
    expectToken(tokens, 4, TokenType::Identifier,  "ieee");
    expectToken(tokens, 5, TokenType::Delimiter,   ".");
    expectToken(tokens, 6, TokenType::Identifier,  "std_logic_1164");
    expectToken(tokens, 7, TokenType::Delimiter,   ".");
    expectToken(tokens, 8, TokenType::Keyword,     "all");
    expectToken(tokens, 9, TokenType::Delimiter,   ";");
    EXPECT_EQ(tokens.size(), 10u);
}

TEST(Tokenizer_Integration, PortMapInstantiation)
{
    // u1 : adder port map ( a => x , b => y , s => sum ) ;
    const std::string src =
        "u1 : adder port map ( a => x , b => y , s => sum ) ;";
    auto tokens = tokenize(src);

    expectToken(tokens, 0,  TokenType::Identifier, "u1");
    expectToken(tokens, 1,  TokenType::Delimiter,  ":");
    expectToken(tokens, 2,  TokenType::Identifier, "adder");
    expectToken(tokens, 3,  TokenType::Keyword,    "port");
    expectToken(tokens, 4,  TokenType::Keyword,    "map");
    expectToken(tokens, 5,  TokenType::Delimiter,  "(");
    expectToken(tokens, 6,  TokenType::Identifier, "a");
    expectToken(tokens, 7,  TokenType::Operator,   "=>");
    expectToken(tokens, 8,  TokenType::Identifier, "x");
    expectToken(tokens, 9,  TokenType::Delimiter,  ",");
}

TEST(Tokenizer_Integration, ConcurrentSignalAssignmentWithBitStringLiteral)
{
    // temp <= x"A_5";
    auto tokens = tokenize("temp <= x\"A_5\";");
    ASSERT_EQ(tokens.size(), 4u);
    expectToken(tokens, 0, TokenType::Identifier,       "temp");
    expectToken(tokens, 1, TokenType::Operator,         "<=");
    expectToken(tokens, 2, TokenType::BitStringLiteral, "x\"A_5\"");
    expectToken(tokens, 3, TokenType::Delimiter,        ";");
}

TEST(Tokenizer_Integration, VectorRangeWithDownto)
{
    // std_logic_vector ( 7 downto 0 )
    auto tokens = tokenize("std_logic_vector(7 downto 0)");
    ASSERT_EQ(tokens.size(), 6u);
    expectToken(tokens, 0, TokenType::StandardType,  "std_logic_vector");
    expectToken(tokens, 1, TokenType::Delimiter,      "(");
    expectToken(tokens, 2, TokenType::NumericLiteral, "7");
    expectToken(tokens, 3, TokenType::Keyword,        "downto");
    expectToken(tokens, 4, TokenType::NumericLiteral, "0");
    expectToken(tokens, 5, TokenType::Delimiter,      ")");
}

