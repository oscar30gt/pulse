// analyzer_names.test.cc — the remaining kinds of names and subtype indications: resolution functions, qualified expressions,
// external names, context clauses, the forms of discrete ranges, statement labels and `open` index constraints.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    const std::string kSignals =
        "signal a, b : std_logic; signal n, m : integer; signal p : boolean; "
        "signal v8 : std_logic_vector(7 downto 0); signal v4 : std_logic_vector(3 downto 0); signal u4 : unsigned(3 downto 0); "
        "type color is (red, green, blue); signal col : color; type pair is record x, y : integer; end record; signal pr : pair; ";

    std::string arch(const std::string& declarations, const std::string& body = "")
    {
        return analysisError(inArchitecture(kSignals + declarations, body));
    }

    std::string proc(const std::string& declarations, const std::string& statements, const std::string& processDeclarations = "")
    {
        return arch(declarations, "process " + processDeclarations + " begin " + statements + " wait; end process;");
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }
}

// ===========================================================================
// 1. RESOLUTION FUNCTIONS
// ===========================================================================

namespace
{
    const std::string kFourValued =
        "type bit4 is ('U', 'Z', '0', '1'); type bit4_vector is array (natural range <>) of bit4; "
        "function resolve(s : bit4_vector) return bit4 is begin return s(s'left); end; ";

    /// Two processes assign the signal `x` of the given type the given values.
    std::string twoDrivers(const std::string& declarations, const std::string& signalType, const std::string& first = "'1'", const std::string& second = "'0'")
    {
        return arch(kFourValued + declarations + "signal x : " + signalType + "; ",
                    "process begin x <= " + first + "; wait; end process; process begin x <= " + second + "; wait; end process;");
    }
}

TEST(Resolution_Functions, AResolvedSubtypeMayHaveSeveralDrivers)
{
    EXPECT_EQ(twoDrivers("subtype rbit is resolve bit4; ", "rbit"), kOk);
    EXPECT_EQ(twoDrivers("", "resolve bit4"), kOk) << "a resolution can be written in the subtype indication of an object";
}

TEST(Resolution_Functions, TheUnresolvedTypeStillHasOneDriver)
{
    const std::string message = twoDrivers("", "bit4");
    EXPECT_TRUE(mentions(message, "cannot have several drivers")) << message;
}

TEST(Resolution_Functions, ASubtypeOfAResolvedSubtypeIsResolved)
{
    EXPECT_EQ(twoDrivers("subtype rbit is resolve bit4; subtype sub is rbit; ", "sub"), kOk);
}

TEST(Resolution_Functions, CompositesOfResolvedTypesAreResolved)
{
    EXPECT_EQ(twoDrivers("subtype rbit is resolve bit4; type rvec is array (0 to 1) of rbit; ", "rvec", "(others => '1')", "(others => '0')"), kOk);
    EXPECT_EQ(twoDrivers("subtype rbit is resolve bit4; type rrec is record f : rbit; end record; ", "rrec", "(f => '1')", "(f => '0')"), kOk);
    EXPECT_TRUE(mentions(twoDrivers("type urec is record f : bit4; end record; ", "urec", "(f => '1')", "(f => '0')"), "cannot have several drivers"));
}

TEST(Resolution_Functions, ElementResolutionOfAnArray)
{
    EXPECT_EQ(twoDrivers("subtype rvec is (resolve) bit4_vector(0 to 1); ", "rvec", "(others => '1')", "(others => '0')"), kOk);
    EXPECT_TRUE(mentions(twoDrivers("", "bit4_vector(0 to 1)", "(others => '1')", "(others => '0')"), "cannot have several drivers"));
}

TEST(Resolution_Functions, ElementResolutionNeedsAnArray)
{
    EXPECT_TRUE(mentions(arch(kFourValued + "subtype bad is (resolve) bit4; "), "needs an array type, but 'bit4' is not one"));
}

TEST(Resolution_Functions, NestedElementResolution)
{
    EXPECT_EQ(arch(kFourValued + "type grid is array (0 to 1) of bit4_vector(0 to 1); subtype rgrid is ((resolve)) grid; "), kOk);
    EXPECT_TRUE(mentions(arch(kFourValued + "subtype bad is ((resolve)) bit4_vector(0 to 1); "), "needs an array type"));
}

TEST(Resolution_Functions, TheFunctionMustBeAResolutionFunctionForTheType)
{
    EXPECT_TRUE(mentions(arch(kFourValued + "function scalar(x : bit4) return bit4 is begin return x; end; subtype bad is scalar bit4; "),
                         "'scalar' is not a resolution function for the type 'bit4'"));
    EXPECT_TRUE(mentions(arch(kFourValued + "function wrong(s : bit4_vector) return integer is begin return 1; end; subtype bad is wrong bit4; "),
                         "is not a resolution function"));
    EXPECT_TRUE(mentions(arch(kFourValued + "type other is ('a', 'b'); subtype bad is resolve other; "), "is not a resolution function for the type 'other'"));
    EXPECT_TRUE(mentions(arch(kFourValued + "function two(s : bit4_vector; t : bit4) return bit4 is begin return t; end; subtype bad is two bit4; "),
                         "is not a resolution function"));
}

TEST(Resolution_Functions, TheNameMustBeAFunction)
{
    EXPECT_TRUE(mentions(arch(kFourValued + "subtype bad is nothing bit4; "), "'nothing' is not a function"));
    EXPECT_TRUE(mentions(arch(kFourValued + "subtype bad is n bit4; "), "'n' is not a function"));
    EXPECT_TRUE(mentions(arch(kFourValued + "procedure hit is begin null; end; subtype bad is hit bit4; "), "is not a resolution function"));
}

TEST(Resolution_Functions, AResolutionFunctionCanBeOverloaded)
{
    const std::string other = "function resolve(s : bit4_vector; x : integer) return bit4 is begin return '0'; end; ";
    EXPECT_EQ(twoDrivers(other + "subtype rbit is resolve bit4; ", "rbit"), kOk);
}

// ===========================================================================
// 2. QUALIFIED EXPRESSIONS
// ===========================================================================

TEST(Qualified_Expressions, TheOperandTakesTheTypeOfTheMark)
{
    EXPECT_EQ(proc("", "a <= std_logic'('1'); v4 <= std_logic_vector'(\"0101\"); u4 <= unsigned'(\"0101\"); n <= integer'(3) + 2; col <= color'(red);"), kOk);
    EXPECT_EQ(proc("", "pr <= pair'(x => 1, y => 2);"), kOk);
    EXPECT_EQ(proc("subtype nib is std_logic_vector(3 downto 0); ", "v4 <= nib'(others => '0');"), kOk);
}

TEST(Qualified_Expressions, ResolveAnAmbiguousLiteral)
{
    const std::string types = "type t1 is (x1, both); type t2 is (x2, both); function k(v : t1) return integer is begin return 1; end; "
                              "function k(v : t2) return integer is begin return 2; end; ";
    EXPECT_TRUE(mentions(arch(types, "process begin n <= k(both); wait; end process;"), "is ambiguous"));
    EXPECT_EQ(arch(types, "process begin n <= k(t1'(both)); n <= k(t2'(both)); wait; end process;"), kOk);
}

TEST(Qualified_Expressions, TheOperandMustBeAValueOfTheType)
{
    EXPECT_TRUE(mentions(proc("", "n <= integer'('1');"), "Type mismatch for the qualified expression of 'integer'"));
    EXPECT_TRUE(mentions(proc("", "a <= std_logic'(n);"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc("", "v4 <= std_logic_vector'(others => '0');"), "'others' can only be used when the target type has a known length"))
        << "an unconstrained mark gives an aggregate no length";
    EXPECT_TRUE(mentions(proc("", "n <= integer'(999999999999);"), "outside the range"));
}

TEST(Qualified_Expressions, ConstrainedTypesCheckTheLength)
{
    EXPECT_EQ(proc("subtype nib is std_logic_vector(3 downto 0); ", "v4 <= nib'(\"0101\");"), kOk);
    EXPECT_TRUE(mentions(proc("subtype nib is std_logic_vector(3 downto 0); ", "v4 <= nib'(\"01\");"), "element"));
}

TEST(Qualified_Expressions, AnUnconstrainedTypeTakesTheBoundsOfItsOperand)
{
    EXPECT_TRUE(mentions(proc("", "v8 <= std_logic_vector'(\"0101\");"), "Length mismatch"));
}

TEST(Qualified_Expressions, TheMarkMustBeAType)
{
    EXPECT_TRUE(mentions(proc("", "n <= nothing'(1);"), "Unknown type 'nothing'"));
    EXPECT_TRUE(mentions(proc("", "n <= m'(1);"), "'m' is not a type"));
}

// ===========================================================================
// 3. EXTERNAL NAMES
// ===========================================================================

TEST(External_Names, ASignalIsReadAndWritten)
{
    EXPECT_EQ(proc("", "a <= <<signal .tb.dut.q : std_logic>>; <<signal .tb.dut.q : std_logic>> <= '1';"), kOk);
    EXPECT_EQ(arch("", "a <= <<signal .tb.q : std_logic>> and b;"), kOk);
}

TEST(External_Names, TheTypeIsTheOneWritten)
{
    EXPECT_TRUE(mentions(proc("", "n <= <<signal .tb.q : std_logic>>;"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc("", "a <= <<signal .tb.q : nothing>>;"), "Unknown type 'nothing'"));
    EXPECT_EQ(proc("", "v4 <= <<signal .tb.q : std_logic_vector(3 downto 0)>>;"), kOk);
    EXPECT_TRUE(mentions(proc("", "v8 <= <<signal .tb.q : std_logic_vector(3 downto 0)>>;"), "Length mismatch"));
}

TEST(External_Names, TheTypeMustBeConstrained)
{
    const std::string message = proc("", "a <= <<signal .tb.q : std_logic_vector>>(0);");
    EXPECT_TRUE(mentions(message, "External name '.tb.q' has the unconstrained type")) << message;
}

TEST(External_Names, ConstantsAreStatic)
{
    EXPECT_EQ(arch("constant k : integer := <<constant .tb.c : integer>>; "), kOk);
    EXPECT_TRUE(mentions(arch("constant k : integer := <<signal .tb.s : integer>>; "), "must be constant"));
    EXPECT_TRUE(mentions(arch("constant k : integer := <<variable .tb.v : integer>>; "), "must be constant"));
}

TEST(External_Names, VariablesAreAssignedWithColonEquals)
{
    EXPECT_EQ(proc("", "<<variable .tb.v : integer>> := 1; n <= <<variable .tb.v : integer>>;"), kOk);
    EXPECT_TRUE(mentions(proc("", "<<variable .tb.v : integer>> <= 1;"), "is a variable; assign it with ':='"));
    EXPECT_TRUE(mentions(proc("", "<<signal .tb.s : integer>> := 1;"), "is a signal; assign it with '<='"));
}

TEST(External_Names, ConstantsCannotBeAssigned)
{
    EXPECT_TRUE(mentions(proc("", "<<constant .tb.c : integer>> <= 1;"), "is a constant and cannot be assigned"));
}

TEST(External_Names, TheSamePathIsTheSameObject)
{
    const std::string message = arch("", "process begin <<signal .tb.s : integer>> <= 1; wait; end process; "
                                         "process begin <<signal .tb.s : integer>> <= 2; wait; end process;");
    EXPECT_TRUE(mentions(message, "cannot have several drivers")) << message;
    EXPECT_EQ(arch("", "process begin <<signal .tb.s : integer>> <= 1; wait; end process; "
                       "process begin <<signal .tb.t : integer>> <= 2; wait; end process;"), kOk);
}

TEST(External_Names, SignalsCanBeWaitedOnAndTested)
{
    EXPECT_EQ(arch("", "process (<<signal .tb.q : std_logic>>) begin a <= '1'; end process;"), kOk);
    EXPECT_EQ(arch("", "process begin wait on <<signal .tb.q : std_logic>>; wait until <<signal .tb.q : std_logic>> = '1'; end process;"), kOk);
    EXPECT_EQ(proc("", "if <<signal .tb.q : std_logic>>'event then null; end if;"), kOk);
    EXPECT_TRUE(mentions(arch("", "process (<<constant .tb.c : integer>>) begin a <= '1'; end process;"), "Only signals and ports can be in a sensitivity list"));
}

TEST(External_Names, IndexedAndSliced)
{
    EXPECT_EQ(proc("", "a <= <<signal .tb.q : std_logic_vector(3 downto 0)>>(2); v4 <= <<signal .tb.q : std_logic_vector(3 downto 0)>>;"), kOk);
    EXPECT_TRUE(mentions(proc("", "a <= <<signal .tb.q : std_logic_vector(3 downto 0)>>(4);"), "out of bounds"));
}

TEST(External_Names, APureFunctionCannotReachOutsideItself)
{
    EXPECT_TRUE(mentions(arch("function f return std_logic is begin return <<signal .tb.q : std_logic>>; end; "), "cannot access '.tb.q'"));
    EXPECT_EQ(arch("function f return integer is begin return <<constant .tb.c : integer>>; end; "), kOk);
}

TEST(External_Names, AsAPortMapActual)
{
    const std::string design =
        "entity dev is port (i : in std_logic; o : out std_logic); end dev; architecture r of dev is begin end r; "
        "entity top is end top; architecture t of top is component dev is port (i : in std_logic; o : out std_logic); end component; begin "
        "u : dev port map (i => <<signal .tb.a : std_logic>>, o => <<signal .tb.b : std_logic>>); end t;";
    EXPECT_EQ(analysisError(design), kOk);
}

// ===========================================================================
// 4. CONTEXT CLAUSES
// ===========================================================================

TEST(Context_Clauses, LibraryAndUseInFrontOfEveryUnit)
{
    EXPECT_EQ(analysisError("library ieee; use ieee.std_logic_1164.all; use ieee.numeric_std.all; entity e is end e; "
                            "library work; use work.stuff.all, work.other.item; architecture a of e is begin end a;"), kOk);
}

TEST(Context_Clauses, UseClausesInDeclarativeParts)
{
    EXPECT_EQ(arch("use work.pkg.all; "), kOk);
    EXPECT_EQ(proc("", "null;", "use work.pkg.all;"), kOk);
    EXPECT_EQ(arch("function f return integer is use work.pkg.all; begin return 1; end; "), kOk);
}

TEST(Context_Clauses, ClausesOfAnEntityDoNotLeakIntoTheArchitecture)
{
    EXPECT_EQ(analysisError("library ieee; entity e is end e; architecture a of e is begin end a;"), kOk);
}

TEST(Context_Clauses, NamesTheyMentionAreNotResolved)
{
    EXPECT_EQ(analysisError("library nothing_like_this; use nothing.at.all; entity e is end e; architecture a of e is begin end a;"), kOk);
}

// ===========================================================================
// 5. THE FORMS OF A DISCRETE RANGE
// ===========================================================================

TEST(Ranges_Forms, TypeMarksAndSubtypeIndicationsAreRanges)
{
    EXPECT_EQ(proc("", "for i in color loop null; end loop; for j in natural range 0 to 3 loop null; end loop; for k in boolean loop null; end loop;"), kOk);
    EXPECT_EQ(proc("subtype small is integer range 0 to 3; ", "for i in small loop n <= i; end loop;"), kOk);
}

TEST(Ranges_Forms, LoopParametersOverATypeTakeThatType)
{
    EXPECT_EQ(proc("", "for i in color loop col <= i; end loop;"), kOk);
    EXPECT_TRUE(mentions(proc("", "for i in color loop n <= i; end loop;"), "Type mismatch"));
}

TEST(Ranges_Forms, ArraysCanBeIndexedByAType)
{
    EXPECT_EQ(arch("type by_color is array (color) of integer; signal tab : by_color; ", "process begin tab(red) <= 1; n <= tab(blue); wait; end process;"), kOk);
    EXPECT_TRUE(mentions(arch("type by_color is array (color) of integer; signal tab : by_color; ", "process begin tab(3) <= 1; wait; end process;"), "Type mismatch"));
    EXPECT_EQ(arch("type by_flag is array (boolean) of integer; signal tab : by_flag; "), kOk);
    EXPECT_EQ(arch("type small_array is array (natural range 0 to 3) of integer; signal tab : small_array; ", "process begin tab(3) <= 1; wait; end process;"), kOk);
}

TEST(Ranges_Forms, SlicesByASubtypeIndication)
{
    EXPECT_EQ(proc("", "v4 <= v8(natural range 3 downto 0);"), kOk) << "the direction written in the subtype indication is kept";
    EXPECT_TRUE(mentions(proc("", "v4 <= v8(natural range 0 to 3);"), "The slice direction 'to' does not match the direction 'downto'"));
    EXPECT_TRUE(mentions(proc("", "v4 <= v8(natural range 9 downto 6);"), "outside the range of 'natural'") == false);
}

TEST(Ranges_Forms, ChoicesMayBeRanges)
{
    EXPECT_EQ(proc("subtype small is integer range 0 to 3; ", "case n is when small => null; when others => null; end case;"), kOk);
    EXPECT_EQ(proc("", "case col is when color => null; end case;"), kOk);
}

TEST(Ranges_Forms, ATypeThatIsNotDiscreteIsNotARange)
{
    EXPECT_TRUE(mentions(proc("", "for i in real loop null; end loop;"), "cannot be used as a range"));
    EXPECT_TRUE(mentions(proc("", "for i in std_logic_vector loop null; end loop;"), "cannot be used as a range"));
    EXPECT_TRUE(mentions(proc("", "for i in n loop null; end loop;"), "Expected a range"));
}

TEST(Ranges_Forms, PhysicalUnitsWithoutAMultiplierAreTheirParentUnit)
{
    const std::string types = "type dist is range 0 to 1000000 units nm; um = 1000 nm; mm = 1000 um; metre = mm; end units; signal d : dist; ";
    EXPECT_EQ(arch(types, "process begin d <= 1 metre; d <= 1 mm; wait; end process;"), kOk);
    EXPECT_TRUE(mentions(arch(types, "process begin d <= 1 metre + 1 km; wait; end process;"), "Unknown unit 'km'"));
    EXPECT_TRUE(mentions(arch(types, "process begin d <= 2000000 metre; wait; end process;"), "outside the range"))
        << "a metre is a millimetre here, so 2000000 of them exceed the range";
}

// ===========================================================================
// 6. OPEN INDEX CONSTRAINTS
// ===========================================================================

TEST(Open_Constraints, OpenLeavesAnArrayUnconstrained)
{
    EXPECT_EQ(arch("subtype vec is std_logic_vector(open); signal s : vec(3 downto 0); "), kOk);
    EXPECT_TRUE(mentions(arch("signal s : std_logic_vector(open); "), "has the unconstrained type"));
}

TEST(Open_Constraints, OpenCannotBeMixedWithRanges)
{
    EXPECT_TRUE(mentions(arch("type matrix is array (natural range <>, natural range <>) of std_logic; subtype half is matrix(open, 0 to 3); "),
                         "'open' cannot be mixed with ranges in an index constraint"));
}

TEST(Open_Constraints, TheNumberOfIndexesMustStillMatch)
{
    EXPECT_TRUE(mentions(arch("subtype bad is std_logic_vector(open, open); "), "has 1 index range(s), but 2 were given"));
}

// ===========================================================================
// 7. LABELS
// ===========================================================================

TEST(Labels_Statements, EveryStatementMayBeLabeled)
{
    EXPECT_EQ(arch("", "first : process begin one : n <= 1; two : if a = '1' then null; end if; three : case n is when others => null; end case; "
                       "four : null; five : wait; end process;"), kOk);
    EXPECT_EQ(arch("", "u : assert p; s : a <= '1'; sel : with n select b <= '1' when 0, '0' when others;"), kOk);
}

TEST(Labels_Statements, LabelsAreUniqueInTheirRegion)
{
    EXPECT_TRUE(mentions(arch("", "process begin l : null; l : null; wait; end process;"), "'l' is already declared"));
    EXPECT_TRUE(mentions(arch("", "l : process begin wait; end process; l : process begin wait; end process;"), "'l' is already declared"));
    EXPECT_EQ(arch("", "l : process begin wait; end process; process begin l : null; wait; end process;"), kOk) << "each process is its own region";
}

TEST(Labels_Statements, ALabelIsNotAValue)
{
    EXPECT_TRUE(mentions(arch("", "l : process begin n <= l; wait; end process;"), "'l' is a label, not a value"));
}

TEST(Labels_Statements, LabelsAndOtherNamesShareARegion)
{
    EXPECT_TRUE(mentions(arch("signal l : integer; ", "l : process begin wait; end process;"), "'l' is already declared"));
}

TEST(Labels_Statements, ExitAndNextNameLoopLabels)
{
    EXPECT_EQ(proc("", "outer : for i in 0 to 1 loop inner : while a = '1' loop exit outer; next inner when b = '0'; end loop inner; end loop outer;"), kOk);
    EXPECT_TRUE(mentions(proc("", "for i in 0 to 1 loop exit nothing; end loop;"), "There is no enclosing loop labeled 'nothing'"));
    EXPECT_TRUE(mentions(proc("", "l : null; for i in 0 to 1 loop exit l; end loop;"), "There is no enclosing loop labeled 'l'")) << "the label is not a loop's";
    EXPECT_TRUE(mentions(proc("", "l : for i in 0 to 1 loop l : for j in 0 to 1 loop null; end loop; end loop;"), "'l' is already"));
}

// ===========================================================================
// 8. FIELDS
// ===========================================================================

TEST(Fields_Access, TheAllSuffixNeedsAnAccessType)
{
    EXPECT_TRUE(mentions(proc("", "n <= pr.all;"), "'.all' needs an access type"));
    EXPECT_TRUE(mentions(proc("", "n <= n.x;"), "cannot be selected"));
}

TEST(Fields_Access, RecordsThroughCallsAndAliases)
{
    EXPECT_EQ(proc("function make return pair is begin return (x => 1, y => 2); end; ", "n <= make.x;"), kOk);
}
