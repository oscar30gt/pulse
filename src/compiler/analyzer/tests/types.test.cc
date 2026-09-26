// types.test.cc — GTest suite for Pulse::Parser semantic analysis of the
// user-defined VHDL type system: type/subtype declarations, scoping, assignment
// compatibility, generic type conversion, and array attribute inference.

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
// 1. TYPE / SUBTYPE DECLARATION VALIDATION
// ===========================================================================

TEST(Analyzer_Types, NumericTypeDeclarationPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte is range 0 to 255;
            signal s : Byte;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, NumericTypeMixedRealIntegerBoundsThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Bad is range 0 to 1.0;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, RealRangeNumericTypePasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Frac is range 0.0 to 1.0;
            signal s : Frac;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, EnumeratedTypeDuplicateLiteralThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Color is (RED, GREEN, RED);
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, PhysicalTypeUnitOfUnknownUnitThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Time2 is range 0 to 100000
                units
                    fs;
                    ps = 1000 unknown_unit;
                end units;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, RecordTypeDuplicateFieldThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
                x : integer;
            end record;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, TypeRedeclaredInSameScopeThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte is range 0 to 255;
            type Byte is range 0 to 15;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, TypeMayShadowPreludeNamePasses)
{
    // Real VHDL allows shadowing a STANDARD-provided identifier from an inner declarative region --
    // the prelude's "integer" is just an ordinary outer-scope entry, not a protected builtin.
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type integer is range 0 to 15;
            signal s : integer;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, SubtypeOfUnknownBaseTypeThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            subtype MyInt is bogus_type;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, SubtypeConstrainedVectorPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            subtype Nibble is std_logic_vector(3 downto 0);
            signal s : Nibble;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, SubtypeWithExtraArgsThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            subtype Nibble is std_logic_vector(3 downto 0);
            signal s : Nibble(1 downto 0);
        begin
        end behavioral;
    )");
}

// ===========================================================================
// 2. ARRAY / RECORD TYPE USAGE
// ===========================================================================

TEST(Analyzer_Types, ConstrainedArraySignalPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte8 is array (0 to 7) of std_logic;
            signal s : Byte8;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, ConstrainedArrayWithArgsThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte8 is array (0 to 7) of std_logic;
            signal s : Byte8(3 downto 0);
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, UnconstrainedArrayWithoutArgsThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type MyVec is array (natural range <>) of std_logic;
            signal s : MyVec;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, UnconstrainedArrayWithArgsPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type MyVec is array (natural range <>) of std_logic;
            signal s : MyVec(7 downto 0);
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, RecordSignalPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
                y : integer;
            end record;
            signal s : Point;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, NestedArrayOfRecordPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
                y : integer;
            end record;
            type Points is array (0 to 3) of Point;
            signal s : Points;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, RecordFieldOfArrayTypePasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte8 is array (0 to 7) of std_logic;
            type Wrapper is record
                data : Byte8;
            end record;
            signal s : Wrapper;
        begin
        end behavioral;
    )");
}

// ===========================================================================
// 3. SCOPING
// ===========================================================================

TEST(Analyzer_Types, ProcessLocalTypeShadowsArchitectureType)
{
    // Redeclaring "Byte" inside the process's own (nested) scope must not collide with the
    // architecture-level "Byte" -- declareType only checks the current scope, proving isolation.
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte is range 0 to 255;
        begin
            process
                type Byte is range 0 to 15;
            begin
            end process;
        end behavioral;
    )");
}

TEST(Analyzer_Types, ProcessLocalTypeNotVisibleOutsideProcess)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal s : Local;
        begin
            process
                type Local is range 0 to 15;
            begin
            end process;
        end behavioral;
    )");
}

// ===========================================================================
// 4. ASSIGNMENT COMPATIBILITY
// ===========================================================================

TEST(Analyzer_Types, SameUserArrayTypeAssignmentPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte8 is array (0 to 7) of std_logic;
            signal a, b : Byte8;
        begin
            a <= b;
        end behavioral;
    )");
}

TEST(Analyzer_Types, DifferentUnconstrainedArrayWidthAssignmentThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type MyVec is array (natural range <>) of std_logic;
            signal a : MyVec(7 downto 0);
            signal b : MyVec(3 downto 0);
        begin
            a <= b;
        end behavioral;
    )");
}

TEST(Analyzer_Types, SameUnconstrainedArrayWidthAssignmentPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type MyVec is array (natural range <>) of std_logic;
            signal a : MyVec(7 downto 0);
            signal b : MyVec(7 downto 0);
        begin
            a <= b;
        end behavioral;
    )");
}

// ===========================================================================
// 5. GENERIC TYPE CONVERSION (CASTS)
// ===========================================================================

TEST(Analyzer_Types, NumericTypeConversionFromIntegerPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte is range 0 to 255;
            signal s : integer;
            signal t : integer;
        begin
            t <= integer(Byte(s));
        end behavioral;
    )");
}

TEST(Analyzer_Types, RealToIntegerConversionPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Frac is range 0.0 to 1.0;
            signal s : Frac;
            signal t : integer;
        begin
            t <= integer(s);
        end behavioral;
    )");
}

TEST(Analyzer_Types, ArrayToIncompatibleTypeConversionThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte8 is array (0 to 7) of std_logic;
            signal s : integer;
            signal t : Byte8;
        begin
            t <= Byte8(s);
        end behavioral;
    )");
}

// ===========================================================================
// 6. ARRAY ATTRIBUTES
// ===========================================================================

TEST(Analyzer_Types, ConstrainedArrayLengthAttributePasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Byte8 is array (0 to 7) of std_logic;
            signal s : Byte8;
            signal len : integer;
        begin
            len <= s'length;
        end behavioral;
    )");
}

TEST(Analyzer_Types, UnconstrainedArrayLeftHighAttributesPass)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type MyVec is array (natural range <>) of std_logic;
            signal s : MyVec(7 downto 0);
            signal l, h : integer;
        begin
            l <= s'left;
            h <= s'high;
        end behavioral;
    )");
}

TEST(Analyzer_Types, LengthAttributeOnRecordThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
            end record;
            signal s : Point;
            signal len : integer;
        begin
            len <= s'length;
        end behavioral;
    )");
}

// ===========================================================================
// 7. THE PRELUDE (no hardcoded types -- std_logic/boolean/etc. are ordinary
//    type declarations, auto-registered and resolved purely through scope)
// ===========================================================================

TEST(Analyzer_Types, PreludeTypesUsableWithNoUserDeclaration)
{
    expectSemanticSuccess(R"(
        entity test is
            port(clk : in std_logic; d : in std_logic_vector(7 downto 0));
        end test;
        architecture behavioral of test is
            signal u : unsigned(7 downto 0);
            signal s : signed(7 downto 0);
            signal i : integer;
            signal r : real;
            signal b : boolean;
        begin
        end behavioral;
    )");
}

TEST(Analyzer_Types, StdLogicNineValueLiteralsPass)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal u, x0, x1, z, w, l, h, dc : std_logic;
        begin
            u  <= 'U';
            x0 <= 'X';
            x1 <= '1';
            z  <= 'Z';
            w  <= 'W';
            l  <= 'L';
            h  <= 'H';
            dc <= '-';
        end behavioral;
    )");
}

TEST(Analyzer_Types, BooleanLiteralAssignmentPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal flag : boolean;
        begin
            flag <= true;
        end behavioral;
    )");
}

TEST(Analyzer_Types, UnsignedStdLogicVectorGenericConversionsPass)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal u : unsigned(7 downto 0);
        begin
            u <= unsigned(a);
            a <= std_logic_vector(u);
        end behavioral;
    )");
}

TEST(Analyzer_Types, RisingEdgeAndToIntegerNoLongerExist)
{
    expectSemanticError(R"(
        entity test is
            port(clk : in std_logic);
        end test;
        architecture behavioral of test is
        begin
            process(clk)
            begin
                if rising_edge(clk) then
                end if;
            end process;
        end behavioral;
    )");

    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            signal a : std_logic_vector(7 downto 0);
            signal i : integer;
        begin
            i <= to_integer(a);
        end behavioral;
    )");
}

// ===========================================================================
// 8. RECORD FIELD ACCESS
// ===========================================================================

TEST(Analyzer_Types, RecordFieldReadAndWritePasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
                y : integer;
            end record;
            signal p : Point;
            signal vx : integer;
        begin
            p.x <= 1;
            vx <= p.x;
        end behavioral;
    )");
}

TEST(Analyzer_Types, RecordUnknownFieldThrows)
{
    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
            end record;
            signal p : Point;
            signal v : integer;
        begin
            v <= p.z;
        end behavioral;
    )");
}

TEST(Analyzer_Types, NestedRecordFieldAccessPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
                y : integer;
            end record;
            type Line is record
                a : Point;
                b : Point;
            end record;
            signal l : Line;
            signal vx : integer;
        begin
            vx <= l.a.x;
        end behavioral;
    )");
}

// ===========================================================================
// 9. GENERIC ENUMERATION LITERALS AND OPERATORS
// ===========================================================================

TEST(Analyzer_Types, EnumLiteralAssignmentPasses)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Color is (RED, GREEN, BLUE);
            signal c : Color;
        begin
            c <= RED;
        end behavioral;
    )");
}

TEST(Analyzer_Types, EnumEqualityAndOrderingPass)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Color is (RED, GREEN, BLUE);
            signal c1, c2 : Color;
            signal eq, lt : boolean;
        begin
            eq <= (c1 = c2);
            lt <= (c1 < c2);
        end behavioral;
    )");
}

TEST(Analyzer_Types, RecordEqualityPassesButOrderingThrows)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
            end record;
            signal p1, p2 : Point;
            signal eq : boolean;
        begin
            eq <= (p1 = p2);
        end behavioral;
    )");

    expectSemanticError(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type Point is record
                x : integer;
            end record;
            signal p1, p2 : Point;
            signal lt : boolean;
        begin
            lt <= (p1 < p2);
        end behavioral;
    )");
}

TEST(Analyzer_Types, SharedEnumLiteralIsResolvedByTheTarget)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type ColorA is (RED, GREEN);
            type ColorB is (RED, BLUE);
            signal c : ColorA;
        begin
            c <= RED;
        end behavioral;
    )");
}

TEST(Analyzer_Types, IndexAndLengthOnNonLogicUserArrayPass)
{
    expectSemanticSuccess(R"(
        entity test is
        end test;
        architecture behavioral of test is
            type IntArray is array (0 to 7) of integer;
            signal arr : IntArray;
            signal v, len : integer;
        begin
            v <= arr(3);
            len <= arr'length;
        end behavioral;
    )");
}
