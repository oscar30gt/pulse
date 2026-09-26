// analyzer.test.cc — GTest suite for Pulse::Parser::analyzeAST
//
// Comprehensive test coverage for VHDL semantic analysis:
// - Type validation and compatibility
// - Scope and symbol resolution
// - Lvalue / writability enforcement
// - Operator, function, and attribute semantics
// - Process and statement rules

#include <gtest/gtest.h>
#include <string>

#include "analyzer.h"
#include "parser.h"
#include "tokenizer.h"

using namespace Pulse::Parser;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void analyzeVHDL(const std::string& source)
{
    Tokenizer tokenizer(source);
    ASTRoot root = VHDLtoAST(tokenizer);
    analyzeAST(root);
}

static void expectSemanticError(const std::string& source)
{
    EXPECT_THROW(analyzeVHDL(source), ast_semantic_error);
}

static void expectSemanticSuccess(const std::string& source)
{
    EXPECT_NO_THROW(analyzeVHDL(source));
}

// ===========================================================================
// 1. TYPE SPECIFICATION VALIDATION (T1 - T6)
// ===========================================================================

TEST(Analyzer_TypeSpec, UnknownTypeNameThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in custom_type);
        end test;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_TypeSpec, StdLogicWithArgsThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in std_logic(7 downto 0));
        end test;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_TypeSpec, IntegerWithArgsThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in integer(7 downto 0));
        end test;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_TypeSpec, BooleanWithArgsThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in boolean(1 downto 0));
        end test;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_TypeSpec, StdLogicVectorNoArgsThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in std_logic_vector);
        end test;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_TypeSpec, StdLogicVectorValidRangesPass)
{
    const std::string source = R"(
        entity test is
            port(
                a : in std_logic_vector(7 downto 0);
                b : out std_logic_vector(0 to 3)
            );
        end test;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_TypeSpec, StdLogicVectorInvertedDowntoThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in std_logic_vector(0 downto 7));
        end test;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_TypeSpec, StdLogicVectorInvertedToThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in std_logic_vector(7 to 0));
        end test;
    )";
    expectSemanticError(source);
}

// ===========================================================================
// 2. SCOPE & SYMBOL RESOLUTION (S1 - S10)
// ===========================================================================

TEST(Analyzer_Scope, UndeclaredSymbolThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
        begin
            a <= 1;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, DuplicatePortNameThrows)
{
    const std::string source = R"(
        entity test is
            port(
                clk : in std_logic;
                clk : out std_logic
            );
        end test;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, DuplicateSignalNameThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic;
            signal a : std_logic;
        begin
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, SignalCollidesWithPortThrows)
{
    const std::string source = R"(
        entity test is
            port(clk : in std_logic);
        end test;
        architecture behavioral of test is
            signal clk : std_logic;
        begin
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, ArchitectureUnknownEntityThrows)
{
    const std::string source = R"(
        architecture behavioral of nonexistent_entity is
        begin
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, ComponentInstantiationUnknownComponentThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
        begin
            u1 : nonexistent_comp port map(a => b);
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, ComponentInstantiationUnknownPortThrows)
{
    const std::string source = R"(
        entity my_comp is
            port(clk : in std_logic);
        end my_comp;
        entity test is
        end test;
        architecture behavioral of test is
            component my_comp is
                port(clk : in std_logic);
            end component;
            signal s : std_logic;
        begin
            u1 : my_comp port map(unknown_port => s);
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, ComponentInstantiationUnknownSignalThrows)
{
    const std::string source = R"(
        entity my_comp is
            port(clk : in std_logic);
        end my_comp;
        entity test is
        end test;
        architecture behavioral of test is
            component my_comp is
                port(clk : in std_logic);
            end component;
        begin
            u1 : my_comp port map(clk => unknown_sig);
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Scope, ComponentInstantiationValidPasses)
{
    const std::string source = R"(
        entity my_comp is
            port(clk : in std_logic);
        end my_comp;
        entity test is
        end test;
        architecture behavioral of test is
            component my_comp is
                port(clk : in std_logic);
            end component;
            signal s : std_logic;
        begin
            u1 : my_comp port map(clk => s);
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 3. LVALUE & WRITABILITY (L1 - L3)
// ===========================================================================

TEST(Analyzer_Lvalue, AssignToInputPortThrows)
{
    const std::string source = R"(
        entity test is
            port(din : in std_logic);
        end test;
        architecture behavioral of test is
        begin
            din <= '1';
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Lvalue, AssignToOutputPortPasses)
{
    const std::string source = R"(
        entity test is
            port(dout : out std_logic);
        end test;
        architecture behavioral of test is
        begin
            dout <= '1';
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Lvalue, AssignToSignalPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal s : std_logic;
        begin
            s <= '0';
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Lvalue, AssignToSlicePasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal v : std_logic_vector(7 downto 0);
        begin
            v(3 downto 0) <= "1010";
            v(7) <= '1';
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 4. TYPE COMPATIBILITY & ASSIGNMENTS (C1 - C2)
// ===========================================================================

TEST(Analyzer_Assignment, StdLogicToStdLogicPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b : std_logic;
        begin
            a <= b;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Assignment, VectorSameWidthPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b : std_logic_vector(7 downto 0);
        begin
            a <= b;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Assignment, VectorDifferentWidthThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal b : std_logic_vector(3 downto 0);
        begin
            a <= b;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Assignment, IntegerToStdLogicThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic;
            signal b : integer;
        begin
            a <= b;
        end behavioral;
    )";
    expectSemanticError(source);
}

// ===========================================================================
// 5. OPERATORS (C5 - C10)
// ===========================================================================

TEST(Analyzer_Operators, ArithmeticOnIntegersPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b, c : integer;
        begin
            c <= a + b * 2 - 5;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Operators, ArithmeticOnLogicThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b, c : std_logic;
        begin
            c <= a + b;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Operators, LogicalOnBooleansPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b, c : boolean;
        begin
            c <= (a and b) or not a;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Operators, LogicalOnLogicSameWidthPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b, c : std_logic_vector(3 downto 0);
        begin
            c <= (a xor b) and not a;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Operators, LogicalOnLogicDifferentWidthThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal b : std_logic_vector(3 downto 0);
            signal c : std_logic_vector(7 downto 0);
        begin
            c <= a and b;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Operators, ShiftOperatorsPass)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b : std_logic_vector(7 downto 0);
        begin
            b <= a sll 2;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Operators, ConcatenationPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic;
            signal b : std_logic_vector(2 downto 0);
            signal c : std_logic_vector(3 downto 0);
        begin
            c <= a & b;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 6. GENERIC TYPE CONVERSION (F1 - F2)
// ===========================================================================
// unsigned(x)/signed(x)/std_logic_vector(x) are no longer hardcoded functions -- they're the
// generic TargetType(expr) conversion applied to prelude-declared types. rising_edge/falling_edge/
// to_unsigned/to_signed/to_integer have no generic equivalent (a predicate and two value-computing
// conversions, not type relabeling) and are simply gone: calling them is now "unknown function".

TEST(Analyzer_Functions, UnsignedAndSignedPass)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal b : unsigned(7 downto 0);
            signal c : signed(7 downto 0);
        begin
            b <= unsigned(a);
            c <= signed(a);
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Functions, RisingEdgeNoLongerExistsThrows)
{
    const std::string source = R"(
        entity test is
            port(clk : in std_logic; q : out std_logic);
        end test;
        architecture behavioral of test is
        begin
            process(clk)
            begin
                if rising_edge(clk) then
                    q <= '1';
                end if;
            end process;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Functions, ToIntegerNoLongerExistsThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal b : integer;
        begin
            b <= to_integer(a);
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Functions, ToUnsignedNoLongerExistsThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : unsigned(7 downto 0);
        begin
            a <= to_unsigned(5, 8);
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Functions, RisingEdgeReplacedByEventIdiomPasses)
{
    // The VHDL idiom that replaces rising_edge(clk): clk'event and clk = '1'.
    const std::string source = R"(
        entity test is
            port(clk : in std_logic; q : out std_logic);
        end test;
        architecture behavioral of test is
        begin
            process(clk)
            begin
                if clk'event and clk = '1' then
                    q <= '1';
                end if;
            end process;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 7. ATTRIBUTES (A1 - A3)
// ===========================================================================

TEST(Analyzer_Attributes, LengthOnVectorPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal l : integer;
        begin
            l <= a'length;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Attributes, LengthOnStdLogicThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic;
            signal l : integer;
        begin
            l <= a'length;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Attributes, EventAttributePasses)
{
    const std::string source = R"(
        entity test is
            port(clk : in std_logic; q : out std_logic);
        end test;
        architecture behavioral of test is
        begin
            process(clk)
            begin
                if clk'event and clk = '1' then
                    q <= '1';
                end if;
            end process;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 8. PROCESS & STATEMENTS (P1, C4, C11, C13, C14)
// ===========================================================================

TEST(Analyzer_Process, SensitivityListWithWaitThrows)
{
    const std::string source = R"(
        entity test is
            port(clk : in std_logic);
        end test;
        architecture behavioral of test is
        begin
            process(clk)
            begin
                wait for 10 ns;
            end process;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Process, UnknownSignalInSensitivityThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
        begin
            process(unknown_clk)
            begin
            end process;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_Process, ProcessWithoutSensitivityWithWaitPasses)
{
    const std::string source = R"(
        entity test is
            port(clk : out std_logic);
        end test;
        architecture behavioral of test is
        begin
            process
            begin
                clk <= '0';
                wait for 10 ns;
                clk <= '1';
                wait for 10 ns;
            end process;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_IfStatement, NonBooleanConditionThrows)
{
    const std::string source = R"(
        entity test is
            port(a : in integer);
        end test;
        architecture behavioral of test is
            signal s : std_logic;
        begin
            process(a)
            begin
                if a then
                    s <= '1';
                end if;
            end process;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_WhenElse, ValidWhenElsePasses)
{
    const std::string source = R"(
        entity test is
            port(
                sel : in boolean;
                a   : in std_logic;
                b   : in std_logic;
                y   : out std_logic
            );
        end test;
        architecture behavioral of test is
        begin
            y <= a when sel else b;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_WhenElse, IncompatibleBranchesThrows)
{
    const std::string source = R"(
        entity test is
            port(
                sel : in boolean;
                a   : in std_logic;
                b   : in integer;
                y   : out std_logic
            );
        end test;
        architecture behavioral of test is
        begin
            y <= a when sel else b;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_WithClause, ValidWithSelectPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal selector, result : integer;
        begin
            with selector select
                result <= 1 when 0,
                          2 when 1,
                          0 when others;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 9. FULL VHDL DESIGN INTEGRATION
// ===========================================================================

TEST(Analyzer_Integration, DFlipFlop)
{
    const std::string source = R"(
        entity dff is
            port(
                clk : in std_logic;
                rst : in std_logic;
                d   : in std_logic;
                q   : out std_logic
            );
        end dff;

        architecture behavioral of dff is
        begin
            process(clk, rst)
            begin
                if rst = '1' then
                    q <= '0';
                elsif clk'event and clk = '1' then
                    q <= d;
                end if;
            end process;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_Integration, Mux4to1)
{
    const std::string source = R"(
        entity mux4 is
            port(
                d0  : in std_logic_vector(7 downto 0);
                d1  : in std_logic_vector(7 downto 0);
                d2  : in std_logic_vector(7 downto 0);
                d3  : in std_logic_vector(7 downto 0);
                sel : in integer;
                y   : out std_logic_vector(7 downto 0)
            );
        end mux4;

        architecture behavioral of mux4 is
        begin
            process(d0, d1, d2, d3, sel)
            begin
                if sel = 0 then
                    y <= d0;
                elsif sel = 1 then
                    y <= d1;
                elsif sel = 2 then
                    y <= d2;
                else
                    y <= d3;
                end if;
            end process;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 10. SIGNED AND UNSIGNED TYPES & CONVERSIONS
// ===========================================================================

TEST(Analyzer_SignedUnsigned, DeclarationsAndRangesPass)
{
    const std::string source = R"(
        entity test is
            port(
                s_in : in signed(7 downto 0);
                u_in : in unsigned(15 downto 0);
                s_out : out signed(7 downto 0);
                u_out : out unsigned(15 downto 0)
            );
        end test;
        architecture behavioral of test is
            signal s_sig : signed(7 downto 0);
            signal u_sig : unsigned(15 downto 0);
        begin
            s_out <= s_in;
            u_out <= u_in;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_SignedUnsigned, LiteralPrefixInference)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal s : signed(7 downto 0);
            signal u : unsigned(7 downto 0);
        begin
            s <= SX"0A";
            u <= UX"0A";
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_SignedUnsigned, LiteralPrefixDoesNotFixTheTypePasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal u : unsigned(7 downto 0);
        begin
            u <= SX"0A";
        end behavioral;
    )";
    // The type of a bit-string literal comes from its context; the S/U prefix only controls extension.
    expectSemanticSuccess(source);
}

TEST(Analyzer_SignedUnsigned, QoLLiteralAssignmentPasses)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal s : signed(7 downto 0);
            signal u : unsigned(7 downto 0);
        begin
            s <= "00001010";
            u <= "00001010";
            s <= X"0A";
            u <= X"0A";
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_SignedUnsigned, NonLiteralVectorAssignmentWithoutCastThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal slv : std_logic_vector(7 downto 0);
            signal s : signed(7 downto 0);
        begin
            s <= slv;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_SignedUnsigned, ExplicitVectorCastsPass)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal slv : std_logic_vector(7 downto 0);
            signal s : signed(7 downto 0);
            signal u : unsigned(7 downto 0);
        begin
            s <= signed(slv);
            u <= unsigned(slv);
            slv <= std_logic_vector(s);
            slv <= std_logic_vector(u);
            s <= signed(u);
            u <= unsigned(s);
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_SignedUnsigned, ArithmeticAndMixedIntegerPass)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal s1, s2, s3 : signed(7 downto 0);
            signal u1, u2, u3 : unsigned(7 downto 0);
            signal i : integer;
        begin
            s3 <= s1 + s2 * 2 - 1;
            u3 <= u1 + u2 + 5;
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_SignedUnsigned, SlvArithmeticThrows)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a, b, c : std_logic_vector(7 downto 0);
        begin
            c <= a + b;
        end behavioral;
    )";
    expectSemanticError(source);
}

TEST(Analyzer_SignedUnsigned, AttributesAndSlicingPass)
{
    const std::string source = R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal s : signed(7 downto 0);
            signal s_slice : signed(3 downto 0);
            signal bit_val : std_logic;
            signal len, left_bound : integer;
        begin
            len <= s'length;
            left_bound <= s'left;
            s_slice <= s(3 downto 0);
            bit_val <= s(2);
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

// ===========================================================================
// 11. ENTITY-COMPONENT CORRELATION
//
// A component is compared with the entity of the same name when an instance of it is bound, which the linker does
// (see linker/tests/linker.test.cc). The analysis only checks the component itself.
// ===========================================================================

TEST(Analyzer_EntityComponent, ComponentPortMismatchIsLeftToTheLinker)
{
    const std::string source = R"(
        entity my_entity is
            port(
                a : in std_logic;
                b : out std_logic
            );
        end my_entity;

        entity top is
        end top;

        architecture behavioral of top is
        begin end behavioral;

        architecture behavioral of my_entity is
            component my_entity is
                port(
                    a : in std_logic;
                    b : out std_logic;
                    c : in std_logic -- Extra port not in entity
                );
            end component;
        begin
        end behavioral;
    )";
    expectSemanticSuccess(source);
}


TEST(Analyzer_EntityComponent, ComponentPortTypeMismatchIsLeftToTheLinker)
{
    const std::string source = R"(
        entity my_entity is
            port(
                a : in std_logic;
                b : out std_logic
            );
        end my_entity;

        entity top is
        end top;

        architecture behavioral of top is
        begin end behavioral;

        architecture behavioral of my_entity is
            component my_entity is
                port(
                    a : in std_logic;
                    b : out std_logic_vector(7 downto 0) -- Type mismatch with entity
                );
            end component;
        begin
        end behavioral;
    )";
    expectSemanticSuccess(source);
}

TEST(Analyzer_EntityComponent, ArchitectureOfAMissingEntityThrows)
{
    const std::string source = R"(
        entity top is
        end top;

        architecture behavioral of top is
        begin end behavioral;

        architecture behavioral of my_entity is
            component my_entity is
                port(
                    a : in std_logic
                );
            end component;
        begin
        end behavioral;
    )";
    expectSemanticError(source);
}
