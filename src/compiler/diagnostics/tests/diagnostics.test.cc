// diagnostics.test.cc — compiler errors formatted the GCC way, and the source lines they quote.

#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "diagnostics.h"
#include "parser.h"

using namespace Pulse::Parser;

namespace
{
    std::optional<std::string> lineOf(const std::string& source, size_t line)
    {
        std::istringstream stream(source);
        return sourceLineOf(stream, line);
    }
}

// ---- Formatting -----------------------------------------------------------------------------------

TEST(Diagnostics_Format, ALocatedErrorNamesTheFileAndQuotesTheLine)
{
    const ast_syntax_error error("Expected ';', but found 'end'", { 22, 9, 0 });
    EXPECT_EQ(formatDiagnostic(error, "project/clock.vhd", "        END IF", false),
              "project/clock.vhd:22:9: syntax error: Expected ';', but found 'end'\n"
              "   22 |         END IF\n"
              "      |         ^\n");
}

TEST(Diagnostics_Format, TheGutterGrowsWithTheLineNumber)
{
    const compiler_error error("message", { 123456, 1, 0 });
    EXPECT_EQ(formatDiagnostic(error, "f.vhd", "x", false),
              "f.vhd:123456:1: compiler error: message\n"
              "123456 | x\n"
              "       | ^\n");
}

TEST(Diagnostics_Format, TheCaretKeepsTheTabsOfTheLine)
{
    const compiler_error error("message", { 1, 4, 0 });
    EXPECT_EQ(formatDiagnostic(error, "f.vhd", "\t\tab", false),
              "f.vhd:1:4: compiler error: message\n"
              "    1 | \t\tab\n"
              "      | \t\t ^\n");
}

TEST(Diagnostics_Format, WithoutASourceLineOnlyTheFirstLineIsWritten)
{
    const ast_lexical_error error("Unterminated string", { 3, 5, 0 });
    EXPECT_EQ(formatDiagnostic(error, "f.vhd", std::nullopt, false), "f.vhd:3:5: lexical error: Unterminated string\n");
}

TEST(Diagnostics_Format, AnErrorWithoutALocationHasNoPrefix)
{
    const compiler_error unknown("Top-level entity 'top' not found", SourceLocation{});
    EXPECT_EQ(formatDiagnostic(unknown, "", std::nullopt, false), "compiler error: Top-level entity 'top' not found\n");

    // A location without a file cannot be shown either.
    const compiler_error noFileKnown("message", { 2, 3, noFile });
    EXPECT_EQ(formatDiagnostic(noFileKnown, "", "text", false), "compiler error: message\n");
}

TEST(Diagnostics_Format, ColorsWrapEachPart)
{
    const ast_syntax_error error("bad", { 1, 1, 0 });
    const std::string text = formatDiagnostic(error, "f.vhd", "x", true);
    EXPECT_NE(text.find("\033[1mf.vhd:1:1:\033[0m"), std::string::npos) << text;
    EXPECT_NE(text.find("\033[1;31msyntax error:\033[0m"), std::string::npos) << text;
    EXPECT_NE(text.find("\033[1mbad\033[0m"), std::string::npos) << text;
    EXPECT_NE(text.find("\033[1;34m^\033[0m"), std::string::npos) << text;
}

TEST(Diagnostics_Format, EveryStageNamesItself)
{
    EXPECT_STREQ(compiler_error("m", {}).stage(), "compiler");
    EXPECT_STREQ(ast_lexical_error("m", {}).stage(), "lexical");
    EXPECT_STREQ(ast_syntax_error("m", {}).stage(), "syntax");
}

// ---- Source lines ---------------------------------------------------------------------------------

TEST(Diagnostics_SourceLines, EveryLineEndingSplitsLines)
{
    const std::string source = "first\nsecond\r\nthird\rfourth";
    EXPECT_EQ(lineOf(source, 1), "first");
    EXPECT_EQ(lineOf(source, 2), "second");
    EXPECT_EQ(lineOf(source, 3), "third");
    EXPECT_EQ(lineOf(source, 4), "fourth");
    EXPECT_EQ(lineOf(source, 5), std::nullopt);
}

TEST(Diagnostics_SourceLines, TheLineAfterAFinalNewlineIsEmpty)
{
    // The end of a file that ends with a newline is located on the empty line after it.
    EXPECT_EQ(lineOf("end;\n", 2), "");
    EXPECT_EQ(lineOf("end;\n", 3), std::nullopt);
    EXPECT_EQ(lineOf("", 1), std::nullopt);
}
