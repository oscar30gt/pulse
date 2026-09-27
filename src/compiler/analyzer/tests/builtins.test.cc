// builtins.test.cc — the IEEE subprograms of the prelude: std_logic_1164 and numeric_std operators and functions, declared
// without a body. Calls and operators resolve against them, their results follow the length rules of the packages, and the
// library records which builtin every call and operator resolved to.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using namespace Pulse::Parser;
using TestUtil::analysisError;
using TestUtil::inArchitecture;
using TestUtil::parseSource;

namespace
{
    constexpr const char* kOk = "<no error>";

    const std::string kSignals =
        "signal clk, x, y : std_logic; signal p : boolean; signal n : integer; signal k : natural; "
        "signal v4 : std_logic_vector(3 downto 0); signal v8 : std_logic_vector(7 downto 0); "
        "signal u4 : unsigned(3 downto 0); signal u8 : unsigned(7 downto 0); signal u16 : unsigned(15 downto 0); "
        "signal s4 : signed(3 downto 0); signal s8 : signed(7 downto 0); signal s16 : signed(15 downto 0); ";

    std::string check(const std::string& body, const std::string& extraDeclarations = "")
    {
        return analysisError(inArchitecture(kSignals + extraDeclarations, "process begin " + body + " wait; end process;"));
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    /// A design analyzed into a library of its own; the library refers to the tree, so both are kept.
    struct Analyzed
    {
        ASTRoot root;
        DesignLibrary library;

        explicit Analyzed(const std::string& source) : root(parseSource(source)) { library.analyze(root); }

        /// The value of the n-th signal assignment of the first process of the first architecture.
        const Expression& assignedValue(size_t n = 0) const
        {
            const auto* process = dynamic_cast<const ProcessStatement*>(TestUtil::firstArchitecture(root)->body.at(0).get());
            size_t seen = 0;
            for (const auto& statement : process->body)
                if (auto* assignment = dynamic_cast<const SignalAssignment*>(statement.get()))
                    if (seen++ == n)
                        return *assignment->value;
            throw std::runtime_error("no such assignment");
        }
    };

    Analyzed analyzed(const std::string& body)
    {
        return Analyzed(inArchitecture(kSignals, "process begin " + body + " wait; end process;"));
    }
}

// ---- std_logic_1164 -------------------------------------------------------------------------------

TEST(Builtins_StdLogic1164, LogicalOperatorsOnStdLogicAndVectors)
{
    EXPECT_EQ(check("x <= x and y; x <= not x; v4 <= v4 xor \"1010\"; v8 <= not v8; x <= and v4; x <= xnor v8;"), kOk);
}

TEST(Builtins_StdLogic1164, LogicalOperatorsNeedOperandsOfOneLength)
{
    const std::string message = check("v8 <= v8 nand v4;");
    EXPECT_TRUE(mentions(message, "the operands must have the same length (8 and 4)")) << message;
}

TEST(Builtins_StdLogic1164, UserArraysOfStdLogicHaveNoLogicalOperators)
{
    const std::string word = "type word is array (0 to 3) of std_logic; signal w : word; ";
    const std::string message = check("w <= w and w;", word);
    EXPECT_TRUE(mentions(message, "'and' is not defined for 'word'")) << message;
    EXPECT_TRUE(mentions(check("w <= not w;", word), "'not' is not defined for 'word'"));
}

TEST(Builtins_StdLogic1164, BooleanLogicStaysPredefined)
{
    EXPECT_EQ(check("p <= p and not p;"), kOk);
}

TEST(Builtins_StdLogic1164, EdgeFunctionsTakeASignal)
{
    EXPECT_EQ(check("p <= rising_edge(clk) or falling_edge(clk);"), kOk);
    const std::string message = check("p <= rising_edge('1');");
    EXPECT_TRUE(mentions(message, "must be a signal")) << message;
}

TEST(Builtins_StdLogic1164, ShiftsOfStdLogicVectors)
{
    EXPECT_EQ(check("v8 <= v8 sll 1; v8 <= v8 ror n;"), kOk);
    EXPECT_NE(check("v8 <= v8 sra 1;"), kOk); // sra is numeric_std's, for unsigned and signed only
}

// ---- numeric_std ----------------------------------------------------------------------------------

TEST(Builtins_NumericStd, ResultLengthsFollowThePackage)
{
    EXPECT_EQ(check("u8 <= u4 + u8; u16 <= u8 * u8; u16 <= u8 * 3; u8 <= u8 / u4; u4 <= u8 mod u4; s8 <= s8 rem 3;"), kOk);
    EXPECT_TRUE(mentions(check("u8 <= u8 * 2;"), "Length mismatch"));
    EXPECT_TRUE(mentions(check("u4 <= u4 mod u8;"), "Length mismatch")); // mod has the length of its right operand
}

TEST(Builtins_NumericStd, ComparisonsWithIntegersGiveBooleans)
{
    EXPECT_EQ(check("p <= u8 > 3; p <= 3 = u8; p <= s8 <= -2; p <= u4 /= u8;"), kOk);
}

TEST(Builtins_NumericStd, ConversionFunctionsHaveTheSizeTheyAreGiven)
{
    EXPECT_EQ(check("u8 <= to_unsigned(5, 8); s4 <= to_signed(-1, s4'length); u16 <= resize(u8, 16); n <= to_integer(s8); k <= to_integer(u8);"), kOk);
    const std::string message = check("u4 <= to_unsigned(5, 8);");
    EXPECT_TRUE(mentions(message, "Length mismatch")) << message;
}

TEST(Builtins_NumericStd, NamedShiftsKeepTheirOperand)
{
    EXPECT_EQ(check("u8 <= shift_left(u8, 2); s8 <= shift_right(s8, k); u8 <= rotate_left(u8, 1) + 1;"), kOk);
    EXPECT_TRUE(mentions(check("u4 <= shift_left(u8, 2);"), "Length mismatch"));
}

TEST(Builtins_NumericStd, NegationAndAbsOfSigned)
{
    EXPECT_EQ(check("s8 <= -s8; s8 <= abs s8;"), kOk);
}

TEST(Builtins_NumericStd, NoOverloadForStdLogicVectorArithmetic)
{
    EXPECT_TRUE(mentions(check("v8 <= v8 + 1;"), "std_logic_vector has no arithmetic"));
    EXPECT_TRUE(mentions(check("n <= to_integer(v8);"), "No overload of 'to_integer'"));
}

TEST(Builtins_NumericStd, ADesignOperatorHidesTheBuiltinWithItsProfile)
{
    const std::string f = "function \"+\"(l, r : unsigned) return unsigned is begin return l; end function; ";
    EXPECT_EQ(check("u4 <= u4 + u8;", f), kOk); // the design's "+" gives the left operand, 4 elements
}

TEST(Builtins_NumericStd, CallsWithBuiltinArgumentsAreStatic)
{
    EXPECT_EQ(analysisError(inArchitecture("constant c : unsigned(3 downto 0) := to_unsigned(9, 4);")), kOk);
}

// ---- What the library records ---------------------------------------------------------------------

TEST(Builtins_Library, OperatorsRecordTheBuiltinTheyResolvedTo)
{
    const Analyzed design = analyzed("u8 <= u8 + 1; n <= n + 1;");

    const auto added = design.library.calleeOf(design.assignedValue(0));
    ASSERT_TRUE(added.has_value());
    EXPECT_EQ(added->name, "\"+\"");
    EXPECT_TRUE(added->builtin);
    EXPECT_EQ(added->parameters.size(), 2u);

    // Integer addition is an implicit operator of the type, not a declared function.
    EXPECT_FALSE(design.library.calleeOf(design.assignedValue(1)).has_value());
}

TEST(Builtins_Library, CallsRecordTheirBuiltinAndItsParameters)
{
    const Analyzed design = analyzed("u16 <= resize(new_size => 16, arg => u8);");
    const auto callee = design.library.calleeOf(design.assignedValue());
    ASSERT_TRUE(callee.has_value());
    EXPECT_EQ(callee->name, "resize");
    EXPECT_TRUE(callee->builtin);
    EXPECT_EQ(callee->parameters, (std::vector<std::string>{ "arg", "new_size" }));
}

TEST(Builtins_Library, ExpressionTypesAreRecorded)
{
    const Analyzed design = analyzed("u16 <= u8 * u8;");
    const SemanticType* type = design.library.typeOf(design.assignedValue());
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(describe(*type), "unsigned(15 downto 0)");
}
