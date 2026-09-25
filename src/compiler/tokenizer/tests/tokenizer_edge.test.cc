// tokenizer_edge.test.cc — edge cases for numeric literals, strings, comments and lexical errors.
//
// Every lexical failure must be an ast_lexical_error (never a bare std::runtime_error or another
// exception type) and must carry a message that names the problem.

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

    std::string lexicalErrorMessage(const std::string& source)
    {
        try
        {
            lex(source);
        }
        catch (const ast_lexical_error& e)
        {
            return e.what();
        }
        return "<no error>";
    }

    struct NumberCase { std::string source; std::string expected; };
}

// ===========================================================================
// 1. NUMERIC LITERALS
// ===========================================================================

TEST(TokenizerEdge_Numbers, ValidFormsAreNormalized)
{
    const std::vector<NumberCase> cases = {
        { "0", "0" }, { "007", "007" }, { "1_000", "1000" }, { "1_0_0", "100" },
        { "3.14", "3.14" }, { "1_0.2_5", "10.25" }, { "1e3", "1e3" }, { "1E+3", "1e+3" },
        { "1.5e-3", "1.5e-3" }, { "16#FF#", "16#ff#" }, { "2#1010#", "2#1010#" },
        { "16#F.8#", "16#f.8#" }, { "16#F_F#", "16#ff#" }, { "8#77#e2", "8#77#e2" },
    };

    for (const auto& c : cases)
    {
        auto tokens = lex(c.source);
        ASSERT_EQ(tokens.size(), 1u) << c.source;
        EXPECT_EQ(tokens[0].type, TokenType::NumericLiteral) << c.source;
        EXPECT_EQ(tokens[0].value, c.expected) << c.source;
    }
}

TEST(TokenizerEdge_Numbers, MalformedFormsThrowLexicalError)
{
    const std::vector<std::string> bad = {
        "10ns",        // needs a space before the unit
        "12abc",       // identifier glued to a number
        "1__0",        // consecutive underscores
        "1_",          // trailing underscore
        "1.",          // decimal point without digits
        "1.e3",        // decimal point without digits before exponent
        "1e",          // exponent without digits
        "1e+",         // exponent sign without digits
        "1e-3",        // integer literal cannot have a negative exponent
        "1.5.3",       // second decimal point
        "16#FF",       // unterminated based literal
        "1#0#",        // base out of range
        "17#1#",       // base out of range
        "2#12#",       // digit not valid in base
        "16#G#",       // digit not valid in base
    };

    for (const auto& source : bad)
        EXPECT_NE(lexicalErrorMessage(source), "<no error>") << "should be rejected: " << source;
}

TEST(TokenizerEdge_Numbers, NumberThenSpacedIdentifierIsTwoTokens)
{
    auto tokens = lex("10 ns");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::NumericLiteral);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "ns");
}

TEST(TokenizerEdge_Numbers, GluedUnitMessageExplainsTheFix)
{
    std::string message = lexicalErrorMessage("10ns");
    EXPECT_NE(message.find("10ns"), std::string::npos) << message;
    EXPECT_NE(message.find("space"), std::string::npos) << message;
}

TEST(TokenizerEdge_Numbers, RangeBoundsAreSeparateTokens)
{
    auto tokens = lex("(7 downto 0)");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[1].value, "7");
    EXPECT_EQ(tokens[2].value, "downto");
    EXPECT_EQ(tokens[3].value, "0");
}

// ===========================================================================
// 2. IDENTIFIERS AND RESERVED WORDS
// ===========================================================================

TEST(TokenizerEdge_Words, ReservedWordsAreNeverIdentifiers)
{
    for (const char* word : { "case", "variable", "after", "generate", "function", "package", "while", "loop",
                              "generic", "assert", "report", "constant", "null", "exit", "next", "all" })
    {
        auto tokens = lex(word);
        ASSERT_EQ(tokens.size(), 1u) << word;
        EXPECT_EQ(tokens[0].type, TokenType::Keyword) << word;
    }
}

TEST(TokenizerEdge_Words, WordOperatorsIncludeArithmeticAndShifts)
{
    for (const char* word : { "abs", "mod", "rem", "and", "nand", "xnor", "sll", "sra", "rol", "downto", "to" })
    {
        auto tokens = lex(word);
        ASSERT_EQ(tokens.size(), 1u) << word;
        EXPECT_EQ(tokens[0].type, TokenType::Operator) << word;
    }
}

TEST(TokenizerEdge_Words, PredefinedNamesStayIdentifiers)
{
    for (const char* word : { "integer", "boolean", "std_logic", "true", "false", "time", "natural", "ns" })
    {
        auto tokens = lex(word);
        ASSERT_EQ(tokens.size(), 1u) << word;
        EXPECT_EQ(tokens[0].type, TokenType::Identifier) << word;
    }
}

// ===========================================================================
// 3. COMMENTS
// ===========================================================================

TEST(TokenizerEdge_Comments, BlockCommentsAreSkipped)
{
    auto tokens = lex("a /* one\n two */ b");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].value, "a");
    EXPECT_EQ(tokens[1].value, "b");
    EXPECT_EQ(tokens[1].line, 2u);
}

TEST(TokenizerEdge_Comments, UnterminatedBlockCommentThrows)
{
    std::string message = lexicalErrorMessage("a /* never closed");
    EXPECT_NE(message.find("block comment"), std::string::npos) << message;
}

TEST(TokenizerEdge_Comments, DivisionIsNotAComment)
{
    auto tokens = lex("a / b /= c");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[1].value, "/");
    EXPECT_EQ(tokens[3].value, "/=");
}

TEST(TokenizerEdge_Comments, LineCommentAtEndOfFileWithoutNewline)
{
    auto tokens = lex("a -- trailing");
    ASSERT_EQ(tokens.size(), 1u);
}

// ===========================================================================
// 4. STRINGS AND BIT STRINGS
// ===========================================================================

TEST(TokenizerEdge_Strings, DoubledQuoteInsideStringIsKept)
{
    auto tokens = lex("\"a\"\"b\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].value, "\"a\"\"b\"");
}

TEST(TokenizerEdge_Strings, PrefixedBodyRejectsCharactersOfOtherBases)
{
    std::string message = lexicalErrorMessage("b\"102\"");
    EXPECT_NE(message.find("binary"), std::string::npos) << message;
}

TEST(TokenizerEdge_Strings, InvalidPrefixMessageNamesThePrefix)
{
    std::string message = lexicalErrorMessage("16y\"1\"");
    EXPECT_NE(message.find("'16y'"), std::string::npos) << message;
}

TEST(TokenizerEdge_Strings, PrefixIsLowercasedBodyKeepsCase)
{
    auto tokens = lex("X\"aB\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].value, "x\"aB\"");
}

// ===========================================================================
// 5. SOURCE TRACKING AND ERRORS
// ===========================================================================

TEST(TokenizerEdge_Positions, CrLfAndTabsTrackLinesAndColumns)
{
    auto tokens = lex("a\r\n\tb");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].line, 1u);
    EXPECT_EQ(tokens[1].line, 2u);
    EXPECT_EQ(tokens[1].column, 2u);
}

TEST(TokenizerEdge_Errors, EveryLexicalErrorIsAnAstError)
{
    const std::vector<std::string> bad = { "$", "10ns", "\"open", "x\"G\"", "/* open", "_x", "a__b" };

    for (const auto& source : bad)
    {
        EXPECT_THROW(lex(source), ast_lexical_error) << source;
        EXPECT_THROW(lex(source), compiler_error) << source;
    }
}

TEST(TokenizerEdge_Errors, UnderscoreStartMessage)
{
    std::string message = lexicalErrorMessage("_x");
    EXPECT_NE(message.find("underscore"), std::string::npos) << message;
}

TEST(TokenizerEdge_Errors, EmptyAndWhitespaceOnlyInputProduceNoTokens)
{
    EXPECT_TRUE(lex("").empty());
    EXPECT_TRUE(lex("  \n\t \r\n").empty());
    EXPECT_TRUE(lex("-- only a comment").empty());
}

// ---- Sized bit strings without a base ----------------------------------------------------

TEST(TokenizerEdge_Numbers, ASizeBeforeAQuoteNeedsABase)
{
    EXPECT_NE(lexicalErrorMessage("x <= 8\"1010\";").find("Invalid bit-string prefix '8'"), std::string::npos);
    EXPECT_NE(lexicalErrorMessage("x <= 16\"FF\";").find("Invalid bit-string prefix '16'"), std::string::npos);
    EXPECT_EQ(lexicalErrorMessage("x <= 8b\"1010\";"), "<no error>");
}

TEST(TokenizerEdge_Numbers, ABitStringSizeMustFitInANumber)
{
    EXPECT_NE(lexicalErrorMessage("x <= 99999999999999999999b\"1\";").find("Bit string size '99999999999999999999' is too large"), std::string::npos);
}

// ---- Bytes that are not well-formed UTF-8 ------------------------------------------------

TEST(TokenizerEdge_Encoding, AStrayLeadByteIsOneCharacterOfAString)
{
    // 0xE9 opens a three-byte sequence in UTF-8, but the next byte is not a continuation byte: it is a single Latin-1 style character.
    const std::vector<Token> tokens = lex(std::string("x <= \"caf\xE9\";"));
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[2].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[2].value, std::string("\"caf\xE9\""));
}

TEST(TokenizerEdge_Encoding, AnUnfinishedSequenceAtTheEndOfTheFileIsStillOneCharacter)
{
    const std::string message = lexicalErrorMessage(std::string("x <= \"caf\xE9"));
    EXPECT_NE(message.find("nterminated"), std::string::npos) << message;
}

TEST(TokenizerEdge_Encoding, WellFormedMultiByteCharactersStayTogether)
{
    const std::vector<Token> tokens = lex(std::string("x <= \"caf\xC3\xA9\";"));
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[2].value, std::string("\"caf\xC3\xA9\""));
}

TEST(TokenizerEdge_Encoding, StrayBytesInCommentsAreSkipped)
{
    EXPECT_EQ(lex(std::string("-- caf\xE9 \xC3\nx")).size(), 1u);
}
