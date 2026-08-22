#include <gtest/gtest.h>
#include <sstream>
#include <memory>
#include "AST.h"
#include "Tokenizer.h"

using namespace Pulse::Parser::VHDL;

// Helper to create a Tokenizer from a string
Tokenizer makeTokenizer(const std::string& vhdl)
{
    std::istringstream inputStream(vhdl);
    return Tokenizer(inputStream);
}

// ============================================================================
// VALID RANGES - should parse without error
// ============================================================================

TEST(ASTBuilderTest, ValidDowntoRangeSimple)
{
    std::string vhdl = R"(
        entity test is
            port (
                clk : in std_logic;
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
            signal temp : std_logic_vector(7 downto 0);
        begin
            temp(7 downto 0) <= data;
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
        EXPECT_GE(root.children.size(), 2);
    });
}

TEST(ASTBuilderTest, ValidToRangeSimple)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
            signal temp : std_logic_vector(0 to 7);
        begin
            temp(0 to 7) <= data;
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
        EXPECT_GE(root.children.size(), 2);
    });
}

TEST(ASTBuilderTest, ValidDowntoRangeSubset)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(15 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(15 downto 8) <= data(7 downto 0);
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidToRangeSubset)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 15)
            );
        end test;

        architecture rtl of test is
        begin
            data(0 to 7) <= data(8 to 15);
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidSingleBitAccess)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(5) <= '0';
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidFullSignalReference)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data <= x"AA";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidDowntoRangeEqual)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(5 downto 5) <= '0';
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidToRangeEqual)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
        begin
            data(3 to 3) <= '1';
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidMultipleAssignmentsWithRanges)
{
    std::string vhdl = R"(
        entity test is
            port (
                a : in std_logic_vector(7 downto 0);
                b : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
            signal x : std_logic_vector(15 downto 0);
            signal y : std_logic_vector(0 to 15);
        begin
            x(15 downto 8) <= a;
            x(7 downto 0) <= a;
            y(0 to 7) <= b;
            y(8 to 15) <= b;
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

// ============================================================================
// INVALID RANGES - should throw error
// ============================================================================

TEST(ASTBuilderTest, InvalidDowntoRangeReversed)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(3 downto 5) <= "11";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidToRangeReversed)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
        begin
            data(7 to 2) <= "111111";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidDowntoNegativeBounds)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(2 downto 5) <= "1111";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidToNegativeBounds)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
        begin
            data(6 to 3) <= "1111";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidDowntoRangeHigherFirst)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(31 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(10 downto 20) <= x"AAAA";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidToRangeLowerFirst)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 31)
            );
        end test;

        architecture rtl of test is
        begin
            data(25 to 10) <= x"AAAA";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidDowntoWithLargeInversion)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(63 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(0 downto 63) <= x"AAAAAAAAAAAAAAAA";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidToWithLargeInversion)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 63)
            );
        end test;

        architecture rtl of test is
        begin
            data(63 to 0) <= x"AAAAAAAAAAAAAAAA";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

// ============================================================================
// ERROR MESSAGE VALIDATION - ensure proper error messages
// ============================================================================

TEST(ASTBuilderTest, DowntoErrorMessageContainsDetails)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(2 downto 8) <= "1111111";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    try
    {
        auto root = VHDLtoAST(tokenizer);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        std::string msg(e.what());
        EXPECT_TRUE(msg.find("downto") != std::string::npos);
        EXPECT_TRUE(msg.find("2") != std::string::npos);
        EXPECT_TRUE(msg.find("8") != std::string::npos);
    }
}

TEST(ASTBuilderTest, ToErrorMessageContainsDetails)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
        begin
            data(8 to 2) <= "1111111";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    try
    {
        auto root = VHDLtoAST(tokenizer);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        std::string msg(e.what());
        EXPECT_TRUE(msg.find("to") != std::string::npos);
        EXPECT_TRUE(msg.find("8") != std::string::npos);
        EXPECT_TRUE(msg.find("2") != std::string::npos);
    }
}

// ============================================================================
// EDGE CASES AND BOUNDARY CONDITIONS
// ============================================================================

TEST(ASTBuilderTest, ValidDowntoZeroBased)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(7 downto 0) <= x"FF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidToZeroBased)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
        begin
            data(0 to 7) <= x"FF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidDowntoNegativeIndices)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(10 downto -5)
            );
        end test;

        architecture rtl of test is
        begin
            data(10 downto 0) <= x"FFFFF";
            data(-1 downto -5) <= x"FFFFF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ValidToNegativeIndices)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(-5 to 10)
            );
        end test;

        architecture rtl of test is
        begin
            data(-5 to 0) <= x"FFFFF";
            data(0 to 10) <= x"FFFFF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, InvalidDowntoWithNegativeIndices)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(10 downto -5)
            );
        end test;

        architecture rtl of test is
        begin
            data(-1 downto 5) <= x"FFFFF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, InvalidToWithNegativeIndices)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(-5 to 10)
            );
        end test;

        architecture rtl of test is
        begin
            data(5 to -1) <= x"FFFFF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

// ============================================================================
// COMPLEX SIGNAL HANDLING
// ============================================================================

TEST(ASTBuilderTest, MultipleSignalsWithDifferentRangeDirections)
{
    std::string vhdl = R"(
        entity test is
            port (
                a : in std_logic_vector(7 downto 0);
                b : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
            signal x : std_logic_vector(7 downto 0);
            signal y : std_logic_vector(0 to 7);
        begin
            x(7 downto 0) <= a;
            y(0 to 7) <= b;
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, NestedComponentWithRanges)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(15 downto 0)
            );
        end test;

        architecture rtl of test is
            component sub is
                port (
                    in_data : in std_logic_vector(7 downto 0);
                    out_data : out std_logic_vector(7 downto 0)
                );
            end component;

            signal internal : std_logic_vector(7 downto 0);
        begin
            inst : sub port map (
                in_data => data(15 downto 8),
                out_data => internal
            );

            data(7 downto 0) <= internal;
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, InvalidRangeInComponentMapping)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(15 downto 0)
            );
        end test;

        architecture rtl of test is
            component sub is
                port (
                    in_data : in std_logic_vector(7 downto 0)
                );
            end component;
        begin
            inst : sub port map (
                in_data => data(5 downto 12)
            );
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

// ============================================================================
// EXPRESSION EVALUATION WITH RANGES
// ============================================================================

TEST(ASTBuilderTest, ValidRangeWithExpressions)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(15 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(15 - 0 downto 8 + 0) <= x"FFFF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, InvalidRangeWithExpressions)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(15 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(8 downto 15 + 0) <= x"FFFF";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

// ============================================================================
// CONDITIONAL ASSIGNMENTS WITH RANGES
// ============================================================================

TEST(ASTBuilderTest, ValidWhenElseWithRanges)
{
    std::string vhdl = R"(
        entity test is
            port (
                sel : in std_logic;
                data : in std_logic_vector(15 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(15 downto 8) <= x"FF" when sel = '1' else x"00";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, InvalidWhenElseWithRanges)
{
    std::string vhdl = R"(
        entity test is
            port (
                sel : in std_logic;
                data : in std_logic_vector(15 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(5 downto 12) <= x"FF" when sel = '1' else x"00";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

// ============================================================================
// ASCII BOUNDARY TESTS (ensure error checking isn't off-by-one)
// ============================================================================

TEST(ASTBuilderTest, DowntoExactlyEqualBounds)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(3 downto 3) <= '0';
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, ToExactlyEqualBounds)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
        begin
            data(3 to 3) <= '0';
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_NO_THROW({
        auto root = VHDLtoAST(tokenizer);
    });
}

TEST(ASTBuilderTest, DowntoOffByOne)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(7 downto 0)
            );
        end test;

        architecture rtl of test is
        begin
            data(3 downto 4) <= "00";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}

TEST(ASTBuilderTest, ToOffByOne)
{
    std::string vhdl = R"(
        entity test is
            port (
                data : in std_logic_vector(0 to 7)
            );
        end test;

        architecture rtl of test is
        begin
            data(4 to 3) <= "00";
        end rtl;
    )";

    auto tokenizer = makeTokenizer(vhdl);
    EXPECT_THROW({
        auto root = VHDLtoAST(tokenizer);
    }, std::runtime_error);
}