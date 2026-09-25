// vhdl2008.test.cc — lexical coverage of VHDL-2008 (IEEE 1076-2008, clause 15).
//
// The tokenizer accepts every valid lexical element even when nothing downstream can simulate it, and rejects
// only text that is not a valid token. These tests cover the parts of the standard beyond the supported subset:
// attribute tick vs character literal, the 2008 operators, the full reserved-word list,
// bit-string rules, token positions and the navigation interface.

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "tokenizer.h"

using namespace Pulse::Parser;

namespace
{
    std::vector<Token> lex(const std::string& source)
    {
        Tokenizer tok(source);
        std::vector<Token> result;
        while (!tok.end())
            result.push_back(*tok.next());
        return result;
    }

    std::vector<std::string> values(const std::string& source)
    {
        std::vector<std::string> result;
        for (const Token& t : lex(source))
            result.push_back(t.value);
        return result;
    }
}

// ===========================================================================
// 1. EXTENDED IDENTIFIERS (not supported)
// ===========================================================================

TEST(Vhdl2008_ExtendedIdentifiers, AreRejectedWithAClearMessage)
{
    try
    {
        lex("\\Foo Bar\\");
        FAIL() << "Expected ast_lexical_error";
    }
    catch (const ast_lexical_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("Extended identifiers"), std::string::npos) << e.what();
        EXPECT_EQ(e.location().column, 1u);
    }
}

// ===========================================================================
// 2. ATTRIBUTE TICK vs CHARACTER LITERAL
// ===========================================================================

TEST(Vhdl2008_Tick, QualifiedExpressionWithCharacterLiteral)
{
    auto tokens = lex("std_logic'('1')");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[1].type, TokenType::Operator);
    EXPECT_EQ(tokens[1].value, "'");
    EXPECT_EQ(tokens[2].value, "(");
    EXPECT_EQ(tokens[3].type, TokenType::CharacterLiteral);
    EXPECT_EQ(tokens[3].value, "'1'");
}

TEST(Vhdl2008_Tick, AttributeAfterIdentifierClosingBracketAndAll)
{
    EXPECT_EQ(lex("clk'event")[1].type, TokenType::Operator);
    EXPECT_EQ(lex("a(1)'length")[4].type, TokenType::Operator);
    EXPECT_EQ(lex("v[integer]'range")[4].type, TokenType::Operator);
    EXPECT_EQ(lex("p.all'length")[3].type, TokenType::Operator);
}

TEST(Vhdl2008_Tick, CharacterLiteralsAfterOperatorsAndKeywords)
{
    EXPECT_EQ(lex("x = '1'")[2].type, TokenType::CharacterLiteral);
    EXPECT_EQ(lex("when '0' =>")[1].type, TokenType::CharacterLiteral);
    EXPECT_EQ(lex("('a','b')")[1].type, TokenType::CharacterLiteral);
}

TEST(Vhdl2008_Tick, QuoteAndSpaceAsCharacterLiterals)
{
    auto tokens = lex("x <= ''' ; y <= ' ';");
    ASSERT_EQ(tokens.size(), 8u);
    EXPECT_EQ(tokens[2].type, TokenType::CharacterLiteral);
    EXPECT_EQ(tokens[2].value, "'''");
    EXPECT_EQ(tokens[6].type, TokenType::CharacterLiteral);
    EXPECT_EQ(tokens[6].value, "' '");
}

// ===========================================================================
// 3. OPERATORS AND DELIMITERS ADDED BY VHDL-2008
// ===========================================================================

TEST(Vhdl2008_Operators, MatchingOperators)
{
    for (const char* op : { "?=", "?/=", "?<", "?<=", "?>", "?>=", "??" })
    {
        auto tokens = lex(std::string("a ") + op + " b");
        ASSERT_EQ(tokens.size(), 3u) << op;
        EXPECT_EQ(tokens[1].type, TokenType::Operator) << op;
        EXPECT_EQ(tokens[1].value, op);
    }
}

TEST(Vhdl2008_Operators, LongestMatchWins)
{
    EXPECT_EQ(values("a?<=b"), (std::vector<std::string>{ "a", "?<=", "b" }));
    EXPECT_EQ(values("a<=b"),  (std::vector<std::string>{ "a", "<=", "b" }));
    EXPECT_EQ(values("a**b"),  (std::vector<std::string>{ "a", "**", "b" }));
}

TEST(Vhdl2008_Operators, ExternalNameBrackets)
{
    EXPECT_EQ(values("<<signal .tb.dut.s : std_logic>>"),
              (std::vector<std::string>{ "<<", "signal", ".", "tb", ".", "dut", ".", "s", ":", "std_logic", ">>" }));
}

TEST(Vhdl2008_Operators, ExternalNamePathDelimiters)
{
    EXPECT_EQ(values("@lib.pkg.^.sig"),
              (std::vector<std::string>{ "@", "lib", ".", "pkg", ".", "^", ".", "sig" }));
}

TEST(Vhdl2008_Operators, ExclamationMarkIsTheVerticalBar)
{
    auto tokens = lex("'a' ! 'b'");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].value, "|");
    EXPECT_EQ(tokens[1].original, "!");
}

TEST(Vhdl2008_Operators, GraveAccentIsADelimiter)
{
    auto tokens = lex("`protect");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::Delimiter);
}

TEST(Vhdl2008_Operators, LoneQuestionMarkIsTheMatchingDelimiter)
{
    // `case?` and `select?` (LRM 15.3 lists `?` as a delimiter).
    EXPECT_EQ(values("case? s is"), (std::vector<std::string>{ "case", "?", "s", "is" }));
    EXPECT_EQ(values("select?"),    (std::vector<std::string>{ "select", "?" }));
}

// ===========================================================================
// 4. RESERVED WORDS
// ===========================================================================

TEST(Vhdl2008_Keywords, ReservedWordsAddedBy2008)
{
    for (const char* word : { "assume", "assume_guarantee", "context", "cover", "default", "fairness", "force",
                              "parameter", "property", "protected", "release", "restrict", "restrict_guarantee",
                              "sequence", "strong", "vmode", "vprop", "vunit" })
    {
        auto tokens = lex(word);
        ASSERT_EQ(tokens.size(), 1u) << word;
        EXPECT_EQ(tokens[0].type, TokenType::Keyword) << word;
    }
}

TEST(Vhdl2008_Keywords, WordFollowedByStringIsNotABitString)
{
    auto tokens = lex("report\"done\"");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::Keyword);
    EXPECT_EQ(tokens[1].type, TokenType::StringLiteral);
}

// ===========================================================================
// 5. NUMERIC LITERAL MATRIX
// ===========================================================================

TEST(Vhdl2008_Numbers, ValidForms)
{
    struct Case { const char* source; const char* value; };
    for (const Case& c : { Case{ "1_000", "1000" }, Case{ "1e3", "1e3" }, Case{ "2#1010#e2", "2#1010#e2" },
                           Case{ "16#F.F#", "16#f.f#" }, Case{ "1.0e-3", "1.0e-3" }, Case{ "16#FF#", "16#ff#" } })
    {
        auto tokens = lex(c.source);
        ASSERT_EQ(tokens.size(), 1u) << c.source;
        EXPECT_EQ(tokens[0].type, TokenType::NumericLiteral) << c.source;
        EXPECT_EQ(tokens[0].value, c.value) << c.source;
        EXPECT_EQ(tokens[0].original, c.source) << c.source;
    }
}

TEST(Vhdl2008_Numbers, InvalidForms)
{
    for (const char* source : { "1.", "1e-3", "16#F#e-1", "10ns", "1__0", "1_", "16#G#", "1#0#", "17#0#", "16#FF", "1e" })
        EXPECT_THROW(lex(source), ast_lexical_error) << source;
}

// ===========================================================================
// 6. BIT STRINGS
// ===========================================================================

TEST(Vhdl2008_BitStrings, UnderscoreMustSitBetweenCharacters)
{
    EXPECT_NO_THROW(lex("b\"1_0\""));
    EXPECT_NO_THROW(lex("x\"F_F_F\""));
    for (const char* source : { "b\"_10\"", "b\"10_\"", "b\"1__0\"", "x\"_F\"" })
        EXPECT_THROW(lex(source), ast_lexical_error) << source;
}

TEST(Vhdl2008_BitStrings, EmptyBodyIsAccepted)
{
    EXPECT_NO_THROW(lex("x\"\""));
}

TEST(Vhdl2008_BitStrings, PlainStringsMayContainAnyGraphicCharacter)
{
    auto tokens = lex("\"a__b _ %\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
}

// ===========================================================================
// 7. POSITIONS
// ===========================================================================

TEST(Vhdl2008_Positions, OriginalKeepsCase)
{
    auto tokens = lex("ENTITY  Foo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].value, "entity");
    EXPECT_EQ(tokens[0].original, "ENTITY");
    EXPECT_EQ(tokens[1].original, "Foo");
    EXPECT_EQ(tokens[1].column, 9u);
}

TEST(Vhdl2008_Positions, NumbersKeepRawTextAndNormalizedValue)
{
    auto tokens = lex("1_000");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].value, "1000");
    EXPECT_EQ(tokens[0].original, "1_000");
}

TEST(Vhdl2008_Positions, CrLfCountsAsOneLineBreak)
{
    auto tokens = lex("a\r\nb\r\n\r\nc");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].line, 2u);
    EXPECT_EQ(tokens[1].column, 1u);
    EXPECT_EQ(tokens[2].line, 4u);
    EXPECT_EQ(tokens[2].column, 1u);
}

TEST(Vhdl2008_Positions, LoneCarriageReturnIsALineBreak)
{
    auto tokens = lex("a\rb -- c\rd");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].line, 2u);
    EXPECT_EQ(tokens[2].line, 3u);
}

TEST(Vhdl2008_Positions, BlockCommentSpanningLinesAdvancesLineNumbers)
{
    auto tokens = lex("a /* x\ny */ b");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[1].line, 2u);
    EXPECT_EQ(tokens[1].column, 6u);
}

TEST(Vhdl2008_Positions, EndOfFileSitsAfterTrailingComment)
{
    Tokenizer tok("a -- trailing\n");
    const Token* eof = tok.eof();
    EXPECT_EQ(eof->type, TokenType::EndOfFile);
    EXPECT_EQ(eof->line, 2u);
    EXPECT_EQ(eof->column, 1u);
}

TEST(Vhdl2008_Positions, EmptyAndCommentOnlyFiles)
{
    EXPECT_EQ(Tokenizer("").size(), 0u);
    EXPECT_EQ(Tokenizer("").eof()->line, 1u);
    EXPECT_EQ(Tokenizer("-- nothing").size(), 0u);
    EXPECT_EQ(Tokenizer("/* nothing */").size(), 0u);
}

TEST(Vhdl2008_Positions, NonBreakingSpaceIsWhitespace)
{
    EXPECT_EQ(lex("a\xC2\xA0" "b").size(), 2u);
}

// ===========================================================================
// 8. NAVIGATION
// ===========================================================================

TEST(Vhdl2008_Navigation, SeekMovesTheCursorForBacktracking)
{
    Tokenizer tok("a b c d");
    tok.next(3);
    EXPECT_EQ(tok.index(), 3u);
    tok.seek(1);
    EXPECT_EQ(tok.peek()->value, "b");
    tok.seek(100);
    EXPECT_TRUE(tok.end());
    EXPECT_EQ(tok.remaining(), 0u);
}

TEST(Vhdl2008_Navigation, PeekAndPrevRespectBothEnds)
{
    Tokenizer tok("a b");
    EXPECT_EQ(tok.peek(-1), nullptr);
    EXPECT_EQ(tok.peek(1)->value, "b");
    EXPECT_EQ(tok.peek(2)->type, TokenType::EndOfFile);
    EXPECT_EQ(tok.prev(), nullptr);
    tok.next(2);
    EXPECT_EQ(tok.peek(-1)->value, "b");
    EXPECT_EQ(tok.next()->type, TokenType::EndOfFile);
}

// ===========================================================================
// 9. NON-ASCII AND CONTROL CHARACTERS
// ===========================================================================

TEST(Vhdl2008_Characters, NonAsciiInsideStringsAndCharacterLiterals)
{
    auto tokens = lex("\"caf\xC3\xA9\" '\xC3\xA9'");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[1].type, TokenType::CharacterLiteral);
    EXPECT_EQ(tokens[1].original, "'é'");
}

TEST(Vhdl2008_Characters, ControlCharacterInStringThrows)
{
    EXPECT_THROW(lex("\"a\tb\""), ast_lexical_error);
}
