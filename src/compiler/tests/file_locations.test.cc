// file_locations.test.cc — every diagnostic of the pipeline names the source file it comes from.
//
// The Tokenizer stamps the index of its file on every token, the parser copies it into every node, and the later stages
// (analysis, linking, elaboration) report the locations of those nodes. Linking and elaboration work on the merged design, so
// only the file index tells which file an error is in.

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

#include "test_helpers.h"

using namespace TestUtil;

namespace
{
    const std::string kContext = "library ieee; use ieee.std_logic_1164.all;\n";

    /// Location of the diagnostic raised while compiling and elaborating `sources`, or nothing.
    std::optional<SourceLocation> errorLocation(const std::vector<std::string>& sources, const std::string& top = "top")
    {
        try
        {
            Simulation simulation(sources, top);
        }
        catch (const compiler_error& e)
        {
            return e.location();
        }
        return std::nullopt;
    }

    /// The first file declares an entity, the second (file 1) uses it.
    const std::string kLeaf = kContext + "entity leaf is port (a : in std_logic; y : out std_logic); end leaf;\n"
                                         "architecture rtl of leaf is begin y <= a; end rtl;\n";
}

// ---- Tokens and nodes ------------------------------------------------------------------------------

TEST(FileLocations, TokensCarryTheirFile)
{
    Tokenizer tokenizer("entity e is\nend e;", 3);
    EXPECT_EQ(tokenizer.peek()->file, 3u);
    tokenizer.next(5);
    EXPECT_EQ(tokenizer.peek()->line, 2u);
    EXPECT_EQ(tokenizer.peek()->file, 3u);
    EXPECT_EQ(tokenizer.eof()->file, 3u);

    Tokenizer anonymous("entity");
    EXPECT_EQ(anonymous.peek()->file, noFile);
}

TEST(FileLocations, LexicalAndSyntaxErrorsCarryTheirFile)
{
    try
    {
        Tokenizer tokenizer("signal $", 4);
        FAIL() << "no lexical error";
    }
    catch (const ast_lexical_error& e)
    {
        EXPECT_EQ(e.location().file, 4u);
        EXPECT_EQ(e.location().column, 8u);
    }

    try
    {
        parseSource("entity e is\nend;;", 2);
        FAIL() << "no syntax error";
    }
    catch (const ast_syntax_error& e)
    {
        EXPECT_EQ(e.location().file, 2u);
        EXPECT_EQ(e.location().line, 2u);
    }
}

TEST(FileLocations, NodesCarryTheirFile)
{
    const ASTRoot root = parseSource("entity e is end e;\narchitecture a of e is begin end a;", 7);
    ASSERT_EQ(root.children.size(), 2u);
    EXPECT_EQ(root.children[0]->source.file, 7u);
    EXPECT_EQ(root.children[1]->source.file, 7u);
    EXPECT_EQ(root.children[1]->source.line, 2u);
}

// ---- Later stages ---------------------------------------------------------------------------------

TEST(FileLocations, AnalysisErrorsNameTheirFile)
{
    const std::string broken = kContext + "entity top is end top;\narchitecture sim of top is\n  signal s : std_logic := 3;\nbegin end sim;\n";
    const auto at = errorLocation({ kLeaf, broken });
    ASSERT_TRUE(at.has_value());
    EXPECT_EQ(at->file, 1u);
    EXPECT_EQ(at->line, 4u);
}

TEST(FileLocations, LinkErrorsNameTheFileOfTheInstance)
{
    const std::string design = kContext + "entity top is end top;\narchitecture sim of top is\n"
                               "  component leaf port (a : in std_logic; y : out std_logic; z : out std_logic); end component;\n"
                               "  signal a, y, z : std_logic;\nbegin\n  u : leaf port map (a, y, z);\nend sim;\n";
    const auto at = errorLocation({ kLeaf, design });
    ASSERT_TRUE(at.has_value());
    EXPECT_EQ(at->file, 1u);
}

TEST(FileLocations, ElaborationErrorsInsideAnInstanceNameTheFileOfItsEntity)
{
    // The unsupported statement is in the architecture of `leaf` (file 1), elaborated from an instance in the top (file 0).
    const std::string leaf = kContext + "entity leaf is port (a : in std_logic); end leaf;\n"
                                        "architecture rtl of leaf is\nbegin\n  assert a = '1';\nend rtl;\n";
    const std::string design = kContext + "entity top is end top;\narchitecture sim of top is\n"
                               "  component leaf port (a : in std_logic); end component;\n  signal a : std_logic;\n"
                               "begin\n  u : leaf port map (a => a);\nend sim;\n";
    const auto at = errorLocation({ design, leaf });
    ASSERT_TRUE(at.has_value());
    EXPECT_EQ(at->file, 1u);
    EXPECT_EQ(at->line, 5u);
}

TEST(FileLocations, ErrorsAboutTheWholeDesignHaveNoLocation)
{
    const auto at = errorLocation({ kLeaf }, "nothing");
    ASSERT_TRUE(at.has_value());
    EXPECT_FALSE(at->known());
}
