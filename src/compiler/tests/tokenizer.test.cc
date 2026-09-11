// tokenizer.test.cc — GTest suite for Pulse::Parser::Tokenizer
//
// Tests are written against the actual implementation:
//   - TokenType variants: Identifier, Keyword, NumericLiteral, BitStringLiteral,
//     CharacterLiteral, Operator, Delimiter, Unknown.
//   - Word operators (and, or, not, ...) produce TokenType::Operator, not Keyword.
//   - All identifiers and keywords are normalised to lower-case.
//   - Plain quoted strings ("...") produce TokenType::BitStringLiteral with default binary radix.
//   - Cursor API: next() / prev() clamp at boundaries and return nullptr.

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
    Tokenizer tok(source);

    std::vector<Token> result;
    result.reserve(tok.size());

    while (const Token* t = tok.next())
        result.push_back(*t);

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

TEST(Tokenizer_Keywords, EveryKeywordRecognised)
{
    // Exhaustive list matching the keyword set in the implementation.
    const std::vector<std::string> kws = {
        "use", "library", "entity", "architecture", "component",
        "is", "of", "begin", "end", "port", "map",
        "in", "out", "inout", "signal", "constant",
        "with", "select", "when", "else", "others", "open",
        "process", "if", "then", "elsif", "wait", "for"
    };

    for (const auto& kw : kws)
    {
        auto tokens = tokenize(kw);
        ASSERT_EQ(tokens.size(), 1u) << "keyword: " << kw;
        EXPECT_EQ(tokens[0].type,  TokenType::Keyword) << "keyword: " << kw;
        EXPECT_EQ(tokens[0].value, kw)                 << "keyword: " << kw;
    }
}

TEST(Tokenizer_Keywords, CaseInsensitive)
{
    auto tokens = tokenize("ENTITY Architecture IS");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::Keyword); EXPECT_EQ(tokens[0].value, "entity");
    EXPECT_EQ(tokens[1].type, TokenType::Keyword); EXPECT_EQ(tokens[1].value, "architecture");
    EXPECT_EQ(tokens[2].type, TokenType::Keyword); EXPECT_EQ(tokens[2].value, "is");
}

TEST(Tokenizer_Keywords, NotInKeywordSetIsIdentifier)
{
    // Words that look keyword-adjacent but are not in the set.
    for (const auto& word : {"generic", "all", "case", "rising_edge"})
    {
        auto tokens = tokenize(word);
        ASSERT_EQ(tokens.size(), 1u) << "word: " << word;
        EXPECT_EQ(tokens[0].type, TokenType::Identifier) << "word: " << word;
    }
}

// ===========================================================================
// 2. WORD OPERATORS  (logical / shift)
// ===========================================================================

// Word operators are tokenised as TokenType::Operator, not Keyword.

TEST(Tokenizer_WordOperators, EveryWordOperatorRecognised)
{
    const std::vector<std::string> ops = {
        "and", "or", "nand", "nor", "xor", "xnor",
        "not", "sll", "srl", "sla", "sra", "rol", "ror"
    };

    for (const auto& op : ops)
    {
        auto tokens = tokenize(op);
        ASSERT_EQ(tokens.size(), 1u) << "word-op: " << op;
        EXPECT_EQ(tokens[0].type,  TokenType::Operator) << "word-op: " << op;
        EXPECT_EQ(tokens[0].value, op)                  << "word-op: " << op;
    }
}

TEST(Tokenizer_WordOperators, CaseInsensitive)
{
    auto tokens = tokenize("AND OR NOT");
    ASSERT_EQ(tokens.size(), 3u);
    for (const auto& t : tokens)
        EXPECT_EQ(t.type, TokenType::Operator);
    EXPECT_EQ(tokens[0].value, "and");
    EXPECT_EQ(tokens[1].value, "or");
    EXPECT_EQ(tokens[2].value, "not");
}

// ===========================================================================
// 3. TWO-CHARACTER OPERATORS
// ===========================================================================

TEST(Tokenizer_TwoCharOperators, AllRecognised)
{
    for (const auto& op : {":=", "<=", ">=", "=>", "/=", "**"})
    {
        auto tokens = tokenize(op);
        ASSERT_EQ(tokens.size(), 1u) << "two-char-op: " << op;
        EXPECT_EQ(tokens[0].type,  TokenType::Operator) << "two-char-op: " << op;
        EXPECT_EQ(tokens[0].value, op)                  << "two-char-op: " << op;
    }
}

TEST(Tokenizer_TwoCharOperators, PriorityOverSingleChar)
{
    // ":=" must not split into ':' then '='.
    auto tokens = tokenize(":=");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].value, ":=");
}

TEST(Tokenizer_TwoCharOperators, SingleCharFallsBackCorrectly)
{
    // "=>" is a two-char op; a lone "=" is a single-char op.
    auto tokens = tokenize("=");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Operator);
    EXPECT_EQ(tokens[0].value, "=");
}

// ===========================================================================
// 4. SINGLE-CHARACTER OPERATORS
// ===========================================================================

TEST(Tokenizer_SingleCharOperators, AllRecognised)
{
    // Full set: & + - * / < > = |
    const std::string ops = "&+-*/<>=|";
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
// 5. DELIMITERS
// ===========================================================================

TEST(Tokenizer_Delimiters, AllRecognised)
{
    // Full set: ( ) [ ] , ; : .
    const std::string delims = "()[],;:.";
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
// 6. IDENTIFIERS
// ===========================================================================

TEST(Tokenizer_Identifiers, Simple)
{
    auto tokens = tokenize("my_signal");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "my_signal");
}

TEST(Tokenizer_Identifiers, NormalisedToLowerCase)
{
    auto tokens = tokenize("MySignal");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "mysignal");
}

TEST(Tokenizer_Identifiers, WithDigits)
{
    auto tokens = tokenize("sig_2");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "sig_2");
}

TEST(Tokenizer_Identifiers, StartsWithUnderscore)
{
    // '_' is a valid start character in this tokenizer.
    auto tokens = tokenize("_hidden");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "_hidden");
}

// ===========================================================================
// 7. NUMERIC LITERALS
// ===========================================================================

TEST(Tokenizer_NumericLiterals, Integer)
{
    auto tokens = tokenize("42");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "42");
}

TEST(Tokenizer_NumericLiterals, Real)
{
    auto tokens = tokenize("3.14");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "3.14");
}

TEST(Tokenizer_NumericLiterals, UnderscoreSeparator)
{
    auto tokens = tokenize("1_000_000");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "1_000_000");
}

TEST(Tokenizer_NumericLiterals, BasedLiteralLowercased)
{
    // 16#FF# — the hex digits are lower-cased.
    auto tokens = tokenize("16#FF#");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "16#ff#");
}

TEST(Tokenizer_NumericLiterals, ExponentNotation)
{
    auto tokens = tokenize("1.5E+3");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::NumericLiteral);
    EXPECT_EQ(tokens[0].value, "1.5e+3");
}

// ===========================================================================
// 8. BIT-STRING LITERALS
// ===========================================================================

TEST(Tokenizer_BitStringLiterals, IdentifierPrefixes)
{
    // Prefixes that are scanned as identifiers then merged with the quoted body.
    const std::vector<std::string> prefixes = {
        "b", "o", "x", "d",
        "ub", "uo", "ux", "ud",
        "sb", "so", "sx", "sd",
        "8b", "8o", "8x", "8d",
        "16x", "32x", "64x", "128x",
        "1x", "2x", "3x", "4x",
        "16sx", "32sx", "64sx", "128sx",
        "1sx", "2sx", "3sx", "4sx",
    };

    for (const auto& pfx : prefixes)
    {
        std::string src = pfx + "\"10\"";
        auto tokens = tokenize(src);
        ASSERT_EQ(tokens.size(), 1u)                           << "prefix: " << pfx;
        EXPECT_EQ(tokens[0].type, TokenType::BitStringLiteral) << "prefix: " << pfx;
        EXPECT_EQ(tokens[0].value, src)                        << "prefix: " << pfx;
    }
}

TEST(Tokenizer_BitStringLiterals, HexWithUnderscoreSeparator)
{
    // Underscores inside the quoted body are preserved as-is.
    auto tokens = tokenize("x\"A_5\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "x\"A_5\"");
}

TEST(Tokenizer_BitStringLiterals, SizedNumericPrefix)
{
    // 8x"FF" — the numeric-size prefix form.
    auto tokens = tokenize("8x\"FF\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "8x\"FF\"");
}

TEST(Tokenizer_BitStringLiterals, UnterminatedIdentifierPrefixThrows)
{
    EXPECT_THROW(tokenize("x\"1010"), std::runtime_error);
}

TEST(Tokenizer_BitStringLiterals, UnterminatedSizedPrefixThrows)
{
    EXPECT_THROW(tokenize("8x\"FF"), std::runtime_error);
}

TEST(Tokenizer_BitStringLiterals, NewlineInsideBodyThrows)
{
    EXPECT_THROW(tokenize("x\"10\n10\""), std::runtime_error);
}

// ===========================================================================
// 9. QUOTED BIT-STRING LITERALS
// ===========================================================================

// Plain quoted strings produce TokenType::BitStringLiteral (default binary radix).

TEST(Tokenizer_StringLiterals, Simple)
{
    auto tokens = tokenize("\"hello\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "\"hello\"");
}

TEST(Tokenizer_StringLiterals, Empty)
{
    auto tokens = tokenize("\"\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::BitStringLiteral);
    EXPECT_EQ(tokens[0].value, "\"\"");
}

TEST(Tokenizer_StringLiterals, UnterminatedThrows)
{
    EXPECT_THROW(tokenize("\"unterminated"), std::runtime_error);
}

TEST(Tokenizer_StringLiterals, NewlineInsideThrows)
{
    EXPECT_THROW(tokenize("\"line1\nline2\""), std::runtime_error);
}

// ===========================================================================
// 10. CHARACTER LITERALS
// ===========================================================================

TEST(Tokenizer_CharacterLiterals, SinglePrintableChar)
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
        ASSERT_EQ(tokens.size(), 1u)                              << "char: " << c;
        EXPECT_EQ(tokens[0].type,  TokenType::CharacterLiteral)   << "char: " << c;
        EXPECT_EQ(tokens[0].value, src)                           << "char: " << c;
    }
}

TEST(Tokenizer_CharacterLiterals, TickWithoutBodyIsOperator)
{
    // A lone tick that doesn't form 'X' -> Operator.
    auto tokens = tokenize("vec'");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "vec");
    EXPECT_EQ(tokens[1].type,  TokenType::Operator);
    EXPECT_EQ(tokens[1].value, "'");
}

TEST(Tokenizer_CharacterLiterals, AttributeTickSequence)
{
    // sig'length  ->  Identifier  Operator(')  Identifier
    auto tokens = tokenize("sig'length");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type,  TokenType::Identifier); EXPECT_EQ(tokens[0].value, "sig");
    EXPECT_EQ(tokens[1].type,  TokenType::Operator);   EXPECT_EQ(tokens[1].value, "'");
    EXPECT_EQ(tokens[2].type,  TokenType::Identifier); EXPECT_EQ(tokens[2].value, "length");
}

// ===========================================================================
// 11. COMMENTS
// ===========================================================================

TEST(Tokenizer_Comments, LineCommentSkipped)
{
    auto tokens = tokenize("signal -- this is ignored\n");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "signal");
}

TEST(Tokenizer_Comments, OnlyCommentsProducesNoTokens)
{
    auto tokens = tokenize("-- nothing here\n-- or here\n");
    EXPECT_EQ(tokens.size(), 0u);
}

TEST(Tokenizer_Comments, DoesNotConsumeNextLine)
{
    auto tokens = tokenize("signal -- comment\nmy_sig");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].value, "signal");
    EXPECT_EQ(tokens[1].value, "my_sig");
}

TEST(Tokenizer_Comments, ImmediatelyAfterKeywordNoSpace)
{
    // "end--comment\n" — the '--' is attached directly to the keyword.
    auto tokens = tokenize("end--comment\n");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type,  TokenType::Keyword);
    EXPECT_EQ(tokens[0].value, "end");
}

// ===========================================================================
// 12. WHITESPACE
// ===========================================================================

TEST(Tokenizer_Whitespace, LeadingAndTrailingIgnored)
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

TEST(Tokenizer_Whitespace, EmptySourceProducesNoTokens)
{
    EXPECT_EQ(tokenize("").size(), 0u);
    EXPECT_EQ(tokenize("   \t\n  ").size(), 0u);
}

// ===========================================================================
// 13. POSITION TRACKING (line / column)
// ===========================================================================

TEST(Tokenizer_PositionTracking, FirstTokenOnFirstLine)
{
    auto tokens = tokenize("entity");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].line,   1u);
    EXPECT_EQ(tokens[0].column, 1u);
}

TEST(Tokenizer_PositionTracking, SecondTokenColumnOnSameLine)
{
    // "entity foo" — 'f' starts at column 8.
    auto tokens = tokenize("entity foo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[1].line,   1u);
    EXPECT_EQ(tokens[1].column, 8u);
}

TEST(Tokenizer_PositionTracking, TokenOnSecondLine)
{
    auto tokens = tokenize("entity\nfoo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[1].line,   2u);
    EXPECT_EQ(tokens[1].column, 1u);
}

TEST(Tokenizer_PositionTracking, ColumnAfterTwoCharOperator)
{
    // ":= x" — 'x' starts at column 4.
    auto tokens = tokenize(":= x");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[1].line,   1u);
    EXPECT_EQ(tokens[1].column, 4u);
}

// ===========================================================================
// 14. ISTREAM CONSTRUCTOR
// ===========================================================================

TEST(Tokenizer_IStream, ProducesIdenticalTokens)
{
    const std::string src = "entity foo is begin end;";

    std::istringstream ss(src);
    Tokenizer tokStream(ss);

    Tokenizer tokString(src);

    ASSERT_EQ(tokStream.size(), tokString.size());

    for (size_t i = 0; i < tokString.size(); ++i)
    {
        const Token* a = tokStream.next();
        const Token* b = tokString.next();
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(a->type,   b->type);
        EXPECT_EQ(a->value,  b->value);
        EXPECT_EQ(a->line,   b->line);
        EXPECT_EQ(a->column, b->column);
    }
}

// ===========================================================================
// 15. CURSOR API  (next / prev / peek)
// ===========================================================================

TEST(Tokenizer_Cursor, InitialState)
{
    Tokenizer tok("a b c");
    EXPECT_EQ(tok.index(),     0u);
    EXPECT_EQ(tok.size(),      3u);
    EXPECT_EQ(tok.remaining(), 3u);
}

TEST(Tokenizer_Cursor, NextAdvancesAndReturnsToken)
{
    Tokenizer tok("a b c");
    const Token* t = tok.next();
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->value, "a");
    EXPECT_EQ(tok.index(), 1u);
}

TEST(Tokenizer_Cursor, NextReturnsNullptrAtEnd)
{
    Tokenizer tok("a");
    tok.next();
    EXPECT_EQ(tok.next(), nullptr);
}

TEST(Tokenizer_Cursor, NextClampsToBoundary)
{
    Tokenizer tok("a b");
    // Advancing by more than remaining must clamp and return nullptr.
    EXPECT_EQ(tok.next(100), nullptr);
    EXPECT_EQ(tok.index(), tok.size());
}

TEST(Tokenizer_Cursor, PrevMovesBackAndReturnsToken)
{
    Tokenizer tok("a b c");
    tok.next(); // index -> 1
    tok.next(); // index -> 2
    // prev() decrements index to 1 and returns tokens[1] = "b"
    const Token* t = tok.prev();
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->value, "b");
    EXPECT_EQ(tok.index(), 1u);
}

TEST(Tokenizer_Cursor, PrevReturnsNullptrAtBeginning)
{
    Tokenizer tok("a b");
    EXPECT_EQ(tok.prev(), nullptr);
}

TEST(Tokenizer_Cursor, PrevClampsToBeginning)
{
    Tokenizer tok("a b");
    tok.next();
    EXPECT_EQ(tok.prev(100), nullptr);
    EXPECT_EQ(tok.index(), 0u);
}

TEST(Tokenizer_Cursor, PeekZeroIsCurrentToken)
{
    Tokenizer tok("a b c");
    tok.next(); // consume 'a', cursor at 1
    const Token* p = tok.peek(0);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->value, "b");
}

TEST(Tokenizer_Cursor, PeekPositiveLooksAhead)
{
    Tokenizer tok("a b c");
    const Token* p = tok.peek(2);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->value, "c");
}

TEST(Tokenizer_Cursor, PeekNegativeLooksBehind)
{
    Tokenizer tok("a b c");
    tok.next(); // cursor -> 1
    tok.next(); // cursor -> 2
    // peek(-1) = tokens[2 - 1] = tokens[1] = "b"
    const Token* p = tok.peek(-1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->value, "b");
}

TEST(Tokenizer_Cursor, PeekReturnsNullptrOutOfBounds)
{
    Tokenizer tok("a");
    EXPECT_EQ(tok.peek(-1), nullptr); // before start
    EXPECT_EQ(tok.peek(1),  nullptr); // past end
}

TEST(Tokenizer_Cursor, PeekDoesNotMoveCursor)
{
    Tokenizer tok("a b c");
    tok.peek(2);
    EXPECT_EQ(tok.index(), 0u);
}

TEST(Tokenizer_Cursor, RemainingDecreasesWithNext)
{
    Tokenizer tok("a b c");
    EXPECT_EQ(tok.remaining(), 3u);
    tok.next();
    EXPECT_EQ(tok.remaining(), 2u);
    tok.next();
    EXPECT_EQ(tok.remaining(), 1u);
    tok.next();
    EXPECT_EQ(tok.remaining(), 0u);
}

// ===========================================================================
// 16. UNKNOWN CHARACTER
// ===========================================================================

TEST(Tokenizer_Unknown, ThrowsOnUnrecognisedCharacter)
{
    EXPECT_THROW(tokenize("@"), std::runtime_error);
}

TEST(Tokenizer_Unknown, ThrowsOnBacktick)
{
    EXPECT_THROW(tokenize("`"), std::runtime_error);
}

TEST(Tokenizer_Unknown, ErrorMessageContainsPosition)
{
    try
    {
        tokenize("\n@");
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        const std::string msg = e.what();
        // Must mention line 2 and column 1.
        EXPECT_NE(msg.find("2"), std::string::npos) << "message: " << msg;
        EXPECT_NE(msg.find("1"), std::string::npos) << "message: " << msg;
    }
}

// ===========================================================================
// 17. INTEGRATION
// ===========================================================================

TEST(Tokenizer_Integration, EntityDeclaration)
{
    const std::string src =
        "entity adder is\n"
        "  port ( a : in std_logic;\n"
        "         b : in std_logic);\n"
        "end adder;\n";

    auto tokens = tokenize(src);

    expectToken(tokens, 0,  TokenType::Keyword,     "entity");
    expectToken(tokens, 1,  TokenType::Identifier,  "adder");
    expectToken(tokens, 2,  TokenType::Keyword,     "is");
    expectToken(tokens, 3,  TokenType::Keyword,     "port");
    expectToken(tokens, 4,  TokenType::Delimiter,   "(");
    expectToken(tokens, 5,  TokenType::Identifier,  "a");
    expectToken(tokens, 6,  TokenType::Delimiter,   ":");
    expectToken(tokens, 7,  TokenType::Keyword,     "in");
    expectToken(tokens, 8,  TokenType::Identifier,  "std_logic");   // plain identifier
    expectToken(tokens, 9,  TokenType::Delimiter,   ";");
    expectToken(tokens, 10, TokenType::Identifier,  "b");
    expectToken(tokens, 11, TokenType::Delimiter,   ":");
    expectToken(tokens, 12, TokenType::Keyword,     "in");
    expectToken(tokens, 13, TokenType::Identifier,  "std_logic");
    expectToken(tokens, 14, TokenType::Delimiter,   ")");
    expectToken(tokens, 15, TokenType::Delimiter,   ";");
    expectToken(tokens, 16, TokenType::Keyword,     "end");
    expectToken(tokens, 17, TokenType::Identifier,  "adder");
    expectToken(tokens, 18, TokenType::Delimiter,   ";");
    EXPECT_EQ(tokens.size(), 19u);
}

TEST(Tokenizer_Integration, SignalAssignment)
{
    auto tokens = tokenize("y <= a and b;");
    ASSERT_EQ(tokens.size(), 6u);
    expectToken(tokens, 0, TokenType::Identifier, "y");
    expectToken(tokens, 1, TokenType::Operator,   "<=");
    expectToken(tokens, 2, TokenType::Identifier, "a");
    expectToken(tokens, 3, TokenType::Operator,   "and");  // word operator -> Operator
    expectToken(tokens, 4, TokenType::Identifier, "b");
    expectToken(tokens, 5, TokenType::Delimiter,  ";");
}

TEST(Tokenizer_Integration, VariableAssignment)
{
    auto tokens = tokenize("count := count + 1;");
    ASSERT_EQ(tokens.size(), 6u);
    expectToken(tokens, 0, TokenType::Identifier,     "count");
    expectToken(tokens, 1, TokenType::Operator,       ":=");
    expectToken(tokens, 2, TokenType::Identifier,     "count");
    expectToken(tokens, 3, TokenType::Operator,       "+");
    expectToken(tokens, 4, TokenType::NumericLiteral, "1");
    expectToken(tokens, 5, TokenType::Delimiter,      ";");
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
    expectToken(tokens, 1,  TokenType::Identifier,       "sel");
    expectToken(tokens, 2,  TokenType::Operator,         "=");
    expectToken(tokens, 3,  TokenType::CharacterLiteral, "'1'");
    expectToken(tokens, 4,  TokenType::Keyword,          "then");
    expectToken(tokens, 5,  TokenType::Identifier,       "y");
    expectToken(tokens, 6,  TokenType::Operator,         "<=");
    expectToken(tokens, 7,  TokenType::Identifier,       "a");
    expectToken(tokens, 8,  TokenType::Delimiter,        ";");
    expectToken(tokens, 9,  TokenType::Keyword,          "else");
    expectToken(tokens, 10, TokenType::Identifier,       "y");
    expectToken(tokens, 11, TokenType::Operator,         "<=");
    expectToken(tokens, 12, TokenType::Identifier,       "b");
    expectToken(tokens, 13, TokenType::Delimiter,        ";");
    expectToken(tokens, 14, TokenType::Keyword,          "end");
    expectToken(tokens, 15, TokenType::Keyword,          "if");
    expectToken(tokens, 16, TokenType::Delimiter,        ";");
    EXPECT_EQ(tokens.size(), 17u);
}

TEST(Tokenizer_Integration, UseLibraryClause)
{
    auto tokens = tokenize("library IEEE;\nuse IEEE.STD_LOGIC_1164.ALL;");

    expectToken(tokens, 0, TokenType::Keyword,    "library");
    expectToken(tokens, 1, TokenType::Identifier, "ieee");
    expectToken(tokens, 2, TokenType::Delimiter,  ";");
    expectToken(tokens, 3, TokenType::Keyword,    "use");
    expectToken(tokens, 4, TokenType::Identifier, "ieee");
    expectToken(tokens, 5, TokenType::Delimiter,  ".");
    expectToken(tokens, 6, TokenType::Identifier, "std_logic_1164");
    expectToken(tokens, 7, TokenType::Delimiter,  ".");
    expectToken(tokens, 8, TokenType::Identifier, "all");    // 'all' is not a keyword here
    expectToken(tokens, 9, TokenType::Delimiter,  ";");
    EXPECT_EQ(tokens.size(), 10u);
}

TEST(Tokenizer_Integration, PortMapInstantiation)
{
    auto tokens = tokenize("u1 : adder port map ( a => x , b => y ) ;");

    expectToken(tokens, 0, TokenType::Identifier, "u1");
    expectToken(tokens, 1, TokenType::Delimiter,  ":");
    expectToken(tokens, 2, TokenType::Identifier, "adder");
    expectToken(tokens, 3, TokenType::Keyword,    "port");
    expectToken(tokens, 4, TokenType::Keyword,    "map");
    expectToken(tokens, 5, TokenType::Delimiter,  "(");
    expectToken(tokens, 6, TokenType::Identifier, "a");
    expectToken(tokens, 7, TokenType::Operator,   "=>");
    expectToken(tokens, 8, TokenType::Identifier, "x");
    expectToken(tokens, 9, TokenType::Delimiter,  ",");
}

TEST(Tokenizer_Integration, BitStringLiteralInAssignment)
{
    auto tokens = tokenize("temp <= x\"A_5\";");
    ASSERT_EQ(tokens.size(), 4u);
    expectToken(tokens, 0, TokenType::Identifier,       "temp");
    expectToken(tokens, 1, TokenType::Operator,         "<=");
    expectToken(tokens, 2, TokenType::BitStringLiteral, "x\"A_5\"");
    expectToken(tokens, 3, TokenType::Delimiter,        ";");
}

TEST(Tokenizer_Integration, VectorRangeWithDownto)
{
    auto tokens = tokenize("std_logic_vector(7 downto 0)");
    ASSERT_EQ(tokens.size(), 6u);
    expectToken(tokens, 0, TokenType::Identifier,     "std_logic_vector");
    expectToken(tokens, 1, TokenType::Delimiter,      "(");
    expectToken(tokens, 2, TokenType::NumericLiteral, "7");
    expectToken(tokens, 3, TokenType::Operator,       "downto");
    expectToken(tokens, 4, TokenType::NumericLiteral, "0");
    expectToken(tokens, 5, TokenType::Delimiter,      ")");
}

TEST(Tokenizer_Integration, CompressedSyntaxDoesNotThrow)
{
    // No spaces between tokens — exercises every adjacency the lexer must handle.
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