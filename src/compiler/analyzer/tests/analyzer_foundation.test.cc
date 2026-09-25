// analyzer_foundation.test.cc — the resolved type system: prelude, subtypes, ranges, constants,
// enumerations, physical types (time), records and components. Every rejected program also
// asserts a fragment of its message, so the diagnostics stay useful.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    /// Analysis result of an architecture with the given declarations and concurrent statements.
    /// The statements run in one process, so they count as a single driver of every signal they assign.
    std::string check(const std::string& declarations, const std::string& body = "")
    {
        const std::string statements = body.empty() ? "" : "process begin " + body + " wait; end process;";
        return analysisError(inArchitecture(declarations, statements));
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }
}

// ---- Prelude --------------------------------------------------------------------------------------

TEST(Semantic_Prelude, EveryPredefinedTypeIsUsable)
{
    EXPECT_EQ(check("signal a : std_logic; signal b : boolean; signal c : integer; signal d : natural; "
                    "signal e : positive; signal f : real; signal g : time; signal h : severity_level; "
                    "signal i : std_logic_vector(7 downto 0); signal j : unsigned(3 downto 0); signal k : signed(3 downto 0);"), kOk);
}

TEST(Semantic_Prelude, UnknownTypeIsReportedByName)
{
    EXPECT_TRUE(mentions(check("signal a : bit;"), "Unknown type 'bit'"));
}

TEST(Semantic_Prelude, ANameThatIsNotATypeIsRejected)
{
    const std::string message = check("signal a : std_logic; signal b : a;");
    EXPECT_TRUE(mentions(message, "'a' is not a type")) << message;
}

TEST(Semantic_Prelude, ScalarTypesRejectIndexConstraints)
{
    EXPECT_TRUE(mentions(check("signal a : std_logic(7 downto 0);"), "not an array"));
    EXPECT_TRUE(mentions(check("signal a : integer(7 downto 0);"), "not an array"));
    EXPECT_TRUE(mentions(check("signal a : boolean(1 downto 0);"), "not an array"));
}

TEST(Semantic_Prelude, ArraysMustBeConstrainedWhenDeclaredAsObjects)
{
    const std::string message = check("signal a : std_logic_vector;");
    EXPECT_TRUE(mentions(message, "unconstrained")) << message;
    EXPECT_TRUE(mentions(message, "7 downto 0")) << message;
}

TEST(Semantic_Prelude, ArrayTypeCannotBeConstrainedTwice)
{
    EXPECT_TRUE(mentions(check("subtype nib is std_logic_vector(3 downto 0); signal a : nib(1 downto 0);"), "already constrained"));
}

// ---- Subtypes -------------------------------------------------------------------------------------

TEST(Semantic_Subtypes, SubtypeIsCompatibleWithItsBaseType)
{
    EXPECT_EQ(check("subtype nib is std_logic_vector(3 downto 0); signal a : nib; signal b : std_logic_vector(3 downto 0);",
                    "b <= a; a <= b;"), kOk);
}

TEST(Semantic_Subtypes, LengthOfASubtypeIsChecked)
{
    const std::string message = check("subtype nib is std_logic_vector(3 downto 0); signal a : nib; signal b : std_logic_vector(7 downto 0);",
                                      "b <= a;");
    EXPECT_TRUE(mentions(message, "Length mismatch")) << message;
    EXPECT_TRUE(mentions(message, "8 element")) << message;
}

TEST(Semantic_Subtypes, NaturalAndIntegerAreTheSameType)
{
    EXPECT_EQ(check("signal n : natural; signal i : integer; signal p : positive;", "i <= n; n <= i; p <= n;"), kOk);
}

TEST(Semantic_Subtypes, DeclarationsAreOrdered)
{
    const std::string message = check("type mem is array (0 to 3) of nib; subtype nib is std_logic_vector(3 downto 0);");
    EXPECT_TRUE(mentions(message, "Unknown type 'nib'")) << message;
}

TEST(Semantic_Subtypes, ElementOfAnArrayCanBeASubtype)
{
    EXPECT_EQ(check("subtype nib is std_logic_vector(3 downto 0); type mem is array (0 to 3) of nib; "
                    "signal m : mem; signal v : std_logic_vector(3 downto 0);", "v <= m(2);"), kOk);
}

TEST(Semantic_Subtypes, UnconstrainedElementTypeIsRejected)
{
    EXPECT_TRUE(mentions(check("type mem is array (0 to 3) of std_logic_vector;"), "must be constrained"));
}

// ---- Shadowing and homographs ---------------------------------------------------------------------

TEST(Semantic_Scopes, RedeclaringAPredefinedTypeShadowsItEverywhereElseUnchanged)
{
    // The user's `integer` is a different type; natural (declared in the prelude) still uses the real one.
    EXPECT_EQ(check("type integer is (one, two); signal s : integer; signal n : natural;", "s <= one; n <= 5;"), kOk);
}

TEST(Semantic_Scopes, ShadowedStdLogicIsADifferentType)
{
    const std::string message = analysisError(
        "entity e is port (p : in std_logic); end e;\n"
        "architecture a of e is type std_logic is ('0', '1'); signal s : std_logic; begin s <= p; end a;");
    EXPECT_TRUE(mentions(message, "Type mismatch")) << message;
}

TEST(Semantic_Scopes, NamesMustBeUniqueInARegion)
{
    EXPECT_TRUE(mentions(check("signal a : integer; signal a : integer;"), "already declared"));
    EXPECT_TRUE(mentions(check("type t is (x, y); signal t : integer;"), "already declared"));
}

TEST(Semantic_Scopes, ObjectsCannotShareANameWithAnEnumerationLiteral)
{
    EXPECT_TRUE(mentions(check("type color is (red, green); signal red : integer;"), "already declared"));
    EXPECT_TRUE(mentions(check("signal red : integer; type color is (red, green);"), "conflicts"));
}

TEST(Semantic_Scopes, PortsAreVisibleInTheArchitecture)
{
    EXPECT_EQ(analysisError("entity e is port (a : in std_logic; y : out std_logic); end e;\n"
                            "architecture r of e is begin y <= a; end r;"), kOk);
}

TEST(Semantic_Scopes, UndeclaredNamesAreReported)
{
    const std::string message = check("signal a : integer;", "a <= missing;");
    EXPECT_TRUE(mentions(message, "'missing' is not declared")) << message;
}

// ---- Ranges and constants -------------------------------------------------------------------------

TEST(Semantic_Ranges, LiteralsAreCheckedAgainstTheRangeOfTheTarget)
{
    EXPECT_EQ(check("signal s : integer range 0 to 15;", "s <= 15;"), kOk);

    const std::string message = check("signal s : integer range 0 to 15;", "s <= 16;");
    EXPECT_TRUE(mentions(message, "16")) << message;
    EXPECT_TRUE(mentions(message, "outside the range")) << message;
}

TEST(Semantic_Ranges, UserIntegerTypesAcceptIntegerLiterals)
{
    EXPECT_EQ(check("type byte is range 0 to 255; signal b : byte;", "b <= 255;"), kOk);
    EXPECT_TRUE(mentions(check("type byte is range 0 to 255; signal b : byte;", "b <= 256;"), "outside the range"));
    EXPECT_TRUE(mentions(check("type byte is range 0 to 255; signal b : byte;", "b <= -1;"), "outside the range"));
}

TEST(Semantic_Ranges, DifferentIntegerTypesAreNotCompatible)
{
    const std::string message = check("type a_t is range 0 to 9; type b_t is range 0 to 9; signal a : a_t; signal b : b_t;", "a <= b;");
    EXPECT_TRUE(mentions(message, "Type mismatch")) << message;
}

TEST(Semantic_Ranges, RealTypesTakeRealLiteralsOnly)
{
    EXPECT_EQ(check("signal r : real;", "r <= 2.5;"), kOk);
    EXPECT_TRUE(mentions(check("signal r : real;", "r <= 2;"), "Type mismatch"));
    EXPECT_TRUE(mentions(check("signal i : integer;", "i <= 2.5;"), "Type mismatch"));
}

TEST(Semantic_Ranges, EmptyAndInvertedRangesAreRejected)
{
    const std::string message = check("signal v : std_logic_vector(0 downto 7);");
    EXPECT_TRUE(mentions(message, "empty")) << message;
    EXPECT_TRUE(mentions(message, "0 to 7")) << message;
    EXPECT_TRUE(mentions(check("signal v : std_logic_vector(7 to 0);"), "empty"));
}

TEST(Semantic_Ranges, NegativeIndicesAreOutsideNatural)
{
    EXPECT_TRUE(mentions(check("signal v : std_logic_vector(-1 downto -8);"), "outside the range of index type"));
}

TEST(Semantic_Ranges, ConstantsFoldIntoRanges)
{
    EXPECT_EQ(check("constant w : integer := 8; signal v : std_logic_vector(w - 1 downto 0); signal u : std_logic_vector(7 downto 0);",
                    "u <= v;"), kOk);

    const std::string message = check("constant w : integer := 8; signal v : std_logic_vector(w * 2 - 1 downto 0); "
                                      "signal u : std_logic_vector(7 downto 0);", "u <= v;");
    EXPECT_TRUE(mentions(message, "Length mismatch")) << message;
}

TEST(Semantic_Ranges, ConstantsCanUseEarlierConstantsAndPowers)
{
    EXPECT_EQ(check("constant w : integer := 3; constant d : integer := 2 ** w; signal v : std_logic_vector(d - 1 downto 0); "
                    "signal u : std_logic_vector(7 downto 0);", "u <= v;"), kOk);
}

TEST(Semantic_Ranges, AttributesOfConstrainedTypesAreStatic)
{
    EXPECT_EQ(check("signal a : std_logic_vector(7 downto 0); signal b : std_logic_vector(a'length - 1 downto 0);", "b <= a;"), kOk);
    EXPECT_EQ(check("signal a : std_logic_vector(7 downto 0); signal b : std_logic_vector(a'range);", "b <= a;"), kOk);
}

TEST(Semantic_Ranges, RangeBoundsMustBeStatic)
{
    const std::string message = check("signal n : integer; signal v : std_logic_vector(n downto 0);");
    EXPECT_TRUE(mentions(message, "constant at analysis time")) << message;
}

TEST(Semantic_Ranges, ConstantValueMustFitItsType)
{
    EXPECT_TRUE(mentions(check("constant c : natural := -1;"), "outside the range"));
}

TEST(Semantic_Ranges, ConstantsAreInitializedByConstantsOnly)
{
    EXPECT_TRUE(mentions(check("signal a : integer; constant c : integer := a;"), "must be constant"));
    EXPECT_TRUE(mentions(check("signal a : integer; signal b : integer := a;"), "must be constant"));
}

TEST(Semantic_Ranges, UnconstrainedConstantTakesItsRangeFromTheValue)
{
    EXPECT_EQ(check("constant c : std_logic_vector := \"0101\"; signal v : std_logic_vector(3 downto 0);", "v <= c;"), kOk);
}

TEST(Semantic_Ranges, ConstantsCannotBeAssigned)
{
    EXPECT_TRUE(mentions(check("constant c : integer := 1;", "c <= 2;"), "constant"));
}

// ---- Enumerations ---------------------------------------------------------------------------------

TEST(Semantic_Enumerations, LiteralsAreValuesOfTheirType)
{
    EXPECT_EQ(check("type color is (red, green, blue); signal c : color;", "c <= green;"), kOk);
}

TEST(Semantic_Enumerations, AnEnumerationLiteralOfAnotherTypeIsRejected)
{
    const std::string message = check("type color is (red, green); type mood is (happy, sad); signal c : color;", "c <= sad;");
    EXPECT_TRUE(mentions(message, "Type mismatch")) << message;
}

TEST(Semantic_Enumerations, SharedLiteralsAreResolvedByTheTarget)
{
    EXPECT_EQ(check("type a_t is (red, green); type b_t is (red, blue); signal a : a_t; signal b : b_t;", "a <= red; b <= red;"), kOk);
}

TEST(Semantic_Enumerations, SharedLiteralWithoutContextIsAmbiguous)
{
    const std::string message = check("type a_t is (red, green); type b_t is (red, blue); signal p : boolean;", "p <= red = red;");
    EXPECT_TRUE(mentions(message, "ambiguous")) << message;
    EXPECT_TRUE(mentions(message, "a_t")) << message;
}

TEST(Semantic_Enumerations, UserCharacterTypesCoexistWithStdLogic)
{
    EXPECT_EQ(check("type flag is ('0', '1'); signal f : flag; signal s : std_logic;", "f <= '1'; s <= '1';"), kOk);
}

TEST(Semantic_Enumerations, DuplicateLiteralsInOneTypeAreRejected)
{
    EXPECT_TRUE(mentions(check("type t is (a, b, a);"), "twice"));
}

// ---- Time and physical types ----------------------------------------------------------------------

TEST(Semantic_Time, PhysicalLiteralsAreTimeValues)
{
    EXPECT_EQ(check("signal t : time := 10 ns; constant c : time := 1 us + 500 ns;", "t <= c; t <= t + 5 ps; t <= 2 * t;"), kOk);
}

TEST(Semantic_Time, EveryPredefinedUnitExists)
{
    EXPECT_EQ(check("signal t : time;", "t <= 1 fs; t <= 1 ps; t <= 1 ns; t <= 1 us; t <= 1 ms; t <= 1 sec; t <= 1 min; t <= 1 hr;"), kOk);
}

TEST(Semantic_Time, FractionalMagnitudesMustBeWholeBaseUnits)
{
    EXPECT_EQ(check("signal t : time;", "t <= 1.5 ns;"), kOk);

    const std::string message = check("signal t : time;", "t <= 1.5 fs;");
    EXPECT_TRUE(mentions(message, "whole number")) << message;
}

TEST(Semantic_Time, UnknownUnitIsReported)
{
    EXPECT_TRUE(mentions(check("signal t : time;", "t <= 5 parsecs;"), "Unknown unit 'parsecs'"));
}

TEST(Semantic_Time, BareNumbersAreNotTime)
{
    const std::string message = check("signal t : time;", "t <= 10;");
    EXPECT_TRUE(mentions(message, "Type mismatch")) << message;
}

TEST(Semantic_Time, WaitForNeedsATimeValue)
{
    EXPECT_EQ(check("signal s : std_logic;", "wait for 5 ns;"), kOk);

    const std::string message = check("signal s : std_logic;", "wait for 5;");
    EXPECT_TRUE(mentions(message, "time")) << message;
    EXPECT_TRUE(mentions(message, "write a unit")) << message;
    EXPECT_TRUE(mentions(check("signal s : std_logic;", "wait for -5 ns;"), "negative"));
}

TEST(Semantic_Time, PhysicalArithmeticRules)
{
    EXPECT_EQ(check("signal t : time; signal n : integer;", "n <= t / t; t <= t / 2; t <= 3 * t;"), kOk);
    EXPECT_TRUE(mentions(check("signal t : time;", "t <= t * t;"), "cannot be applied"));
    EXPECT_TRUE(mentions(check("signal t : time;", "t <= t + 1;"), "cannot be applied"));
}

TEST(Semantic_Time, UserPhysicalTypes)
{
    const std::string decl = "type length is range 0 to 1000000 units mm; cm = 10 mm; m = 100 cm; end units; signal l : length;";
    EXPECT_EQ(check(decl, "l <= 5 cm + 2 mm;"), kOk);
    EXPECT_TRUE(mentions(check(decl + " signal t : time;", "l <= 5 ns;"), "Type mismatch"));
}

TEST(Semantic_Time, UnitDefinitionsAreValidated)
{
    EXPECT_TRUE(mentions(check("type t is range 0 to 9 units u; v = 2 w; end units;"), "not an earlier unit"));
    EXPECT_TRUE(mentions(check("type t is range 0 to 9 units u; u = 2 u; end units;"), "twice"));
}

TEST(Semantic_Time, UnitsAreSingleValuesOfTheirType)
{
    EXPECT_EQ(check("signal t : time;", "t <= ns;"), kOk);
}

// ---- Records --------------------------------------------------------------------------------------

TEST(Semantic_Records, FieldAccessAndAggregates)
{
    const std::string decl = "type pt is record x : integer; y : std_logic; end record; signal p : pt; signal n : integer; signal s : std_logic;";
    EXPECT_EQ(check(decl, "n <= p.x; s <= p.y; p.x <= 3; p <= (x => 1, y => '1'); p <= (2, '0');"), kOk);
}

TEST(Semantic_Records, UnknownFieldListsTheAvailableOnes)
{
    const std::string message = check("type pt is record x : integer; y : std_logic; end record; signal p : pt; signal n : integer;", "n <= p.z;");
    EXPECT_TRUE(mentions(message, "no field 'z'")) << message;
    EXPECT_TRUE(mentions(message, "fields: x, y")) << message;
}

TEST(Semantic_Records, AggregateMustAssignEveryField)
{
    const std::string message = check("type pt is record x : integer; y : std_logic; end record; signal p : pt;", "p <= (x => 1);");
    EXPECT_TRUE(mentions(message, "does not assign field(s) y")) << message;
}

TEST(Semantic_Records, FieldTypesAreChecked)
{
    EXPECT_TRUE(mentions(check("type pt is record x : integer; end record; signal p : pt;", "p <= (x => '1');"), "Type mismatch"));
}

TEST(Semantic_Records, NestedRecordsAndArrayFields)
{
    EXPECT_EQ(check("type inner is record v : std_logic_vector(3 downto 0); end record; type outer is record i : inner; end record; "
                    "signal o : outer; signal b : std_logic;", "b <= o.i.v(2);"), kOk);
}

TEST(Semantic_Records, DuplicateFieldsAreRejected)
{
    EXPECT_TRUE(mentions(check("type pt is record x : integer; x : integer; end record;"), "twice"));
}

// ---- Components -----------------------------------------------------------------------------------

TEST(Semantic_Components, ComponentMustMatchItsEntity)
{
    const std::string entity = "entity adder is port (a : in std_logic_vector(7 downto 0); s : out std_logic_vector(7 downto 0)); end adder;\n";
    const auto design = [&](const std::string& componentPorts)
    {
        return analysisError(entity + "entity top is end top; architecture r of top is component adder port (" + componentPorts
                             + "); end component; begin end r;");
    };

    EXPECT_EQ(design("a : in std_logic_vector(7 downto 0); s : out std_logic_vector(7 downto 0)"), kOk);
    EXPECT_TRUE(mentions(design("x : in std_logic"), "does not exist"));
    EXPECT_TRUE(mentions(design("a : in std_logic_vector(3 downto 0)"), "has type"));
    EXPECT_TRUE(mentions(design("a : out std_logic_vector(7 downto 0)"), "mode"));
}

TEST(Semantic_Components, ComponentWithoutEntityIsRejected)
{
    EXPECT_TRUE(mentions(check("component ghost port (a : in std_logic); end component;"), "no entity"));
}

// ---- Structure ------------------------------------------------------------------------------------

TEST(Semantic_Structure, DuplicatePortsAndArchitectureOfUnknownEntity)
{
    EXPECT_TRUE(mentions(analysisError("entity e is port (a : in std_logic; a : out std_logic); end e;"), "declared twice"));
    EXPECT_TRUE(mentions(analysisError("architecture r of nothing is begin end r;"), "unknown entity 'nothing'"));
}

TEST(Semantic_Structure, InputPortsCannotBeAssigned)
{
    const std::string message = analysisError("entity e is port (a : in std_logic); end e;\narchitecture r of e is begin a <= '1'; end r;");
    EXPECT_TRUE(mentions(message, "input")) << message;
}

TEST(Semantic_Structure, OutputPortsCanBeReadInVhdl2008)
{
    EXPECT_EQ(analysisError("entity e is port (y : out std_logic; z : out std_logic); end e;\n"
                            "architecture r of e is begin y <= '1'; z <= y; end r;"), kOk);
}
