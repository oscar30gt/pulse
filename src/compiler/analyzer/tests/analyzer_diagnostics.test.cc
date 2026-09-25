// analyzer_diagnostics.test.cc — the less common accepting and rejecting paths of the analyzer, one test per rule:
// aggregates of every shape, multi-dimensional arrays, record and array formals of subprograms, aliases, attribute
// specifications of every entity class, choices, range constraints of every scalar class, units, operators on vectors and
// physical values, function return paths, and the structure of a design.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    const std::string kSignals =
        "signal a, b, c, d : std_logic; signal n, m : integer; signal p, q : boolean; signal t : time; "
        "signal v4, w4 : std_logic_vector(3 downto 0); signal v2 : std_logic_vector(1 downto 0); signal v8 : std_logic_vector(7 downto 0); "
        "signal u4, x4 : unsigned(3 downto 0); signal s4 : signed(3 downto 0); signal u8 : unsigned(7 downto 0); "
        "type color is (red, green, blue); signal col : color; ";

    std::string arch(const std::string& declarations, const std::string& body = "")
    {
        return analysisError(inArchitecture(kSignals + declarations, body));
    }

    std::string proc(const std::string& statements, const std::string& declarations = "", const std::string& processDeclarations = "")
    {
        return arch(declarations, "process " + processDeclarations + " begin " + statements + " wait; end process;");
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    const std::string kPair = "type pair is record x, y : integer; end record; ";
    const std::string kMatrix = "type mat is array (0 to 1, 0 to 1) of integer; ";
}

// ===========================================================================
// 1. AGGREGATES
// ===========================================================================

TEST(Diagnostics_Aggregates, AnUnconstrainedTargetStartsAtTheLeftOfItsIndexType)
{
    // `std_logic_vector` is indexed by `natural`, so the aggregate runs upwards from 0.
    EXPECT_EQ(arch("constant c1 : std_logic_vector := ('1', '0', '1'); constant l : integer range 0 to 0 := c1'left; "
                   "constant r : integer range 2 to 2 := c1'right; constant h : integer range 2 to 2 := c1'high; constant k : integer range 3 to 3 := c1'length;"), kOk);
}

TEST(Diagnostics_Aggregates, NamedElementsOfAnUnconstrainedTargetGiveItsBounds)
{
    EXPECT_EQ(arch("constant c1 : std_logic_vector := (2 => '1', 3 => '0', 4 => '1'); constant l : integer range 2 to 2 := c1'left; "
                   "constant r : integer range 4 to 4 := c1'right;"), kOk);
    EXPECT_EQ(arch("constant c1 : std_logic_vector := (4 => '1', 2 => '1', 3 => '0'); constant k : integer range 3 to 3 := c1'length;"), kOk);
}

TEST(Diagnostics_Aggregates, AnUnconstrainedTargetNeedsToKnowWhereItStarts)
{
    EXPECT_TRUE(mentions(arch("constant c1 : std_logic_vector := (others => '1');"), "'others' can only be used when the target type has a known length"));
    EXPECT_TRUE(mentions(arch("constant c1 : std_logic_vector := ('1', 3 => '0');"), "Positional and named elements cannot be mixed"));
    EXPECT_TRUE(mentions(arch("constant c1 : std_logic_vector := (1 => '1', 1 => '0');"), "Index 1 is defined twice in the aggregate"));
    EXPECT_TRUE(mentions(arch("constant c1 : std_logic_vector := (1 => '1', 3 => '0');"), "leave gaps between 1 and 3"));
}

TEST(Diagnostics_Aggregates, PositionalElementsComeFirst)
{
    EXPECT_TRUE(mentions(arch("constant k : std_logic_vector(1 downto 0) := (0 => '1', '0');"), "Positional elements must come before named elements"));
}

TEST(Diagnostics_Aggregates, RecordAggregates)
{
    EXPECT_EQ(arch(kPair + "constant k : pair := (x => 1, y => 2);"), kOk);
    EXPECT_EQ(arch(kPair + "constant k : pair := (1, 2);"), kOk);
    EXPECT_EQ(arch(kPair + "constant k : pair := (y => 2, x => 1);"), kOk);
    EXPECT_EQ(arch(kPair + "constant k : pair := (x => 1, others => 2);"), kOk) << "others takes the remaining fields";
    EXPECT_EQ(arch(kPair + "constant k : pair := (others => 2);"), kOk);
    EXPECT_EQ(arch(kPair + "constant k : pair := (x | y => 2);"), kOk);
}

TEST(Diagnostics_Aggregates, RecordAggregateErrors)
{
    EXPECT_TRUE(mentions(arch(kPair + "constant k : pair := (x => 1, x => 2);"), "Field 'x' is assigned twice in the aggregate"));
    EXPECT_TRUE(mentions(arch(kPair + "constant k : pair := (1, 2, 3);"), "The aggregate has more elements than record 'pair' has fields"));
    EXPECT_TRUE(mentions(arch(kPair + "constant k : pair := (zz => 1, y => 2);"), "Record type 'pair' has no field named in this choice"));
    EXPECT_TRUE(mentions(arch(kPair + "constant k : pair := (1 => 1, y => 2);"), "Record type 'pair' has no field named in this choice"));
    EXPECT_TRUE(mentions(arch(kPair + "constant k : pair := (x => 1);"), "does not assign field(s) y of record 'pair'"));
    EXPECT_TRUE(mentions(arch(kPair + "constant k : pair := (x => '1', y => 2);"), "field 'x' of record 'pair'"));
}

TEST(Diagnostics_Aggregates, MultiDimensionalArraysAreIndexedButNotBuiltFromAggregates)
{
    EXPECT_TRUE(mentions(arch(kMatrix + "constant k : mat := ((1, 2), (3, 4));"), "Aggregates of multi-dimensional arrays are not supported yet"));
    EXPECT_TRUE(mentions(arch(kMatrix + "constant k : mat := \"00\";"), "cannot initialize the multi-dimensional type"));
}

// ===========================================================================
// 2. MULTI-DIMENSIONAL ARRAYS
// ===========================================================================

TEST(Diagnostics_MultiDimensional, IndexingNeedsOneValuePerDimension)
{
    EXPECT_EQ(arch(kMatrix + "signal k : mat; signal e : integer;", "e <= k(0, 1);"), kOk);
    EXPECT_TRUE(mentions(arch(kMatrix + "signal k : mat; signal e : integer;", "e <= k(0);"), "needs 2 index value(s), but 1 were given"));
    EXPECT_TRUE(mentions(arch(kMatrix + "signal k : mat; signal e : integer;", "e <= k(0, 1, 1);"), "needs 2 index value(s), but 3 were given"));
    EXPECT_TRUE(mentions(arch(kMatrix + "signal k : mat; signal e : integer;", "e <= k(0, 5);"), "Index 5 is out of bounds"));
}

TEST(Diagnostics_MultiDimensional, WholeArraysAssignButSlicesAreNotSupported)
{
    EXPECT_EQ(arch(kMatrix + "signal k, l : mat;", "l <= k;"), kOk);
    EXPECT_TRUE(mentions(arch(kMatrix + "signal k, l : mat;", "l <= k(0 to 1);"), "Slices of multi-dimensional arrays are not supported yet"));
}

TEST(Diagnostics_MultiDimensional, ConstraintsNeedOneRangePerDimension)
{
    EXPECT_TRUE(mentions(arch(kMatrix + "signal k : mat(0 to 1);"), "is already constrained"));
    EXPECT_TRUE(mentions(arch("type um is array (natural range <>, natural range <>) of integer; signal k : um(0 to 1);"),
                         "has 2 index range(s), but 1 were given"));
    EXPECT_EQ(arch("type um is array (natural range <>, natural range <>) of integer; signal k : um(0 to 1, 0 to 2);"), kOk);
}

// ===========================================================================
// 3. FORMALS OF SUBPROGRAMS: RECORDS, ELEMENTS, SLICES AND CONVERSIONS
// ===========================================================================

TEST(Diagnostics_Formals, RecordFieldsAreAssociatedOneByOne)
{
    const std::string declarations = kPair + "signal r : pair; procedure pr(signal s : out pair) is begin null; end; ";
    EXPECT_EQ(arch(declarations, "pr(s.x => n, s.y => m);"), kOk);
    EXPECT_EQ(arch(declarations, "pr(s.x => n);"), kOk) << "an output need not be covered";
    EXPECT_EQ(arch(declarations, "pr(s => r);"), kOk);
    EXPECT_TRUE(mentions(arch(declarations, "pr(s.x => n, s.x => m);"), "Field 'x' of 's' is associated more than once"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s.zz => n, s.y => m);"), "Record type 'pair' has no field 'zz'"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s(1) => n);"), "cannot be indexed or sliced"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s.x.z => n, s.y => m);"), "'z' cannot be selected: the formal has type 'integer', which is not a record"));
}

TEST(Diagnostics_Formals, AnInputRecordNeedsEveryFieldWhenAssociatedByParts)
{
    const std::string declarations = kPair + "procedure pr(constant s : in pair) is begin null; end; ";
    EXPECT_EQ(arch(declarations, "pr(s.x => 1, s.y => 2);"), kOk);
    EXPECT_TRUE(mentions(arch(declarations, "pr(s.x => 1);"), "Field 'y' of 's' is not associated"));
}

TEST(Diagnostics_Formals, ElementsOfAFieldCannotBeDecidedHere)
{
    const std::string declarations = "type row is array (0 to 1) of integer; type holder is record r : row; end record; "
                                     "procedure pr(signal s : out holder) is begin null; end; ";
    EXPECT_EQ(arch(declarations, "pr(s.r(0) => n, s.r(1) => m);"), kOk);
}

TEST(Diagnostics_Formals, ArrayElementsAndSlices)
{
    const std::string declarations = "procedure pr(signal s : out std_logic_vector(3 downto 0)) is begin null; end; "
                                     "procedure pi(constant s : in std_logic_vector(3 downto 0)) is begin null; end; ";
    EXPECT_EQ(arch(declarations, "pr(s(0) => a, s(1) => b, s(2) => c, s(3) => d);"), kOk);
    EXPECT_EQ(arch(declarations, "pr(s(0) => a, s(1) => b);"), kOk);
    EXPECT_EQ(arch(declarations, "pr(s(3 downto 2) => v2, s(1 downto 0) => v2);"), kOk);
    EXPECT_EQ(arch(declarations, "pr(s(n) => a);"), kOk) << "an index that is not constant cannot be decided";
    EXPECT_EQ(arch(declarations, "pr(s(3 downto 2)(1) => a);"), kOk);

    EXPECT_TRUE(mentions(arch(declarations, "pr(s(0) => a, s(0) => b);"), "Element 0 of 's' is associated more than once"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s(9) => a);"), "Element 9 is outside 's'"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s(3 downto 1) => v2, s(1 downto 0) => v2);"), "Element 1 of 's' is associated more than once"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s(1, 2) => a);"), "needs 1 index value(s), but 2 were given"));
    EXPECT_TRUE(mentions(arch(declarations, "pi(s(0) => '1', s(1) => '0');"), "Only 2 of the 4 elements of 's' are associated"));
    EXPECT_EQ(arch(declarations, "pi(s(0) => '1', s(1) => '0', s(2) => '1', s(3) => '0');"), kOk);
}

TEST(Diagnostics_Formals, ScalarsHaveNoParts)
{
    const std::string declarations = "procedure pr(signal s : out integer) is begin null; end; ";
    EXPECT_TRUE(mentions(arch(declarations, "pr(s.x => n);"), "'x' cannot be selected: the formal has type 'integer', which is not a record"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s(1) => n);"), "A value of type 'integer' cannot be indexed or sliced"));
}

TEST(Diagnostics_Formals, TheFormalMustBeAParameter)
{
    const std::string declarations = "procedure pr(signal s : in std_logic; signal r : out std_logic) is begin null; end; ";
    EXPECT_TRUE(mentions(arch(declarations, "pr(a, b(1) => c);"), "no parameter named 'b'"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s => a, zz(r) => b);"), "The conversion of a formal must name a type or a function"));
}

TEST(Diagnostics_Formals, AFormalMustBeBuiltOnAName)
{
    const std::string declarations = "procedure pr(signal s : out std_logic) is begin null; end; ";
    EXPECT_TRUE(mentions(arch(declarations, "pr(1 => a);"), "This is not a valid formal in 'pr'"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(s'event => a);"), "This is not a valid formal in 'pr'"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(\"s\" => a);"), "This is not a valid formal in 'pr'"));
    EXPECT_EQ(arch(declarations, "pr((s) => a);"), kOk) << "parentheses around a formal are ignored";
}

TEST(Diagnostics_Formals, ConversionsOnOutputFormals)
{
    const std::string integers = "function ident(x : integer) return integer is begin return x; end; procedure pr(signal x : out integer) is begin null; end; ";
    EXPECT_EQ(arch(integers, "pr(ident(x) => n);"), kOk);
    EXPECT_EQ(arch(integers, "pr(integer(x) => n);"), kOk);
    EXPECT_TRUE(mentions(arch(integers, "pr(ident(x) => c);"), "Type mismatch for parameter 'x' of 'pr'"));
    EXPECT_TRUE(mentions(arch(integers, "pr(a(x) => n);"), "'a' is neither a type nor a function, so it cannot convert a formal"));
    EXPECT_TRUE(mentions(arch(integers, "pr(n(x) => n);"), "'n' is neither a type nor a function"));

    const std::string overloaded = "function ident(x : integer) return integer is begin return x; end; "
                                   "function ident(x : std_logic) return integer is begin return 1; end; "
                                   "procedure pr(signal x : out integer) is begin null; end; ";
    EXPECT_EQ(arch(overloaded, "pr(ident(x) => n);"), kOk) << "only one ident accepts an integer";

    const std::string ambiguous = "function ident(x : integer) return integer is begin return x; end; "
                                  "function ident(y : integer) return std_logic is begin return '1'; end; "
                                  "procedure pr(signal x : out integer) is begin null; end; ";
    EXPECT_TRUE(mentions(arch(ambiguous, "pr(ident(x) => n);"), "ident"));

    EXPECT_TRUE(mentions(arch("function ident(x : std_logic) return integer is begin return 1; end; procedure pr(signal x : out integer) is begin null; end; ",
                              "pr(ident(x) => n);"), "No function 'ident' converts a value of type 'integer'"));
}

TEST(Diagnostics_Formals, ConversionsOfArrays)
{
    const std::string declarations = "procedure pr(signal x : out std_logic_vector(3 downto 0)) is begin null; end; ";
    EXPECT_EQ(arch(declarations, "pr(unsigned(x) => u4);"), kOk);
    EXPECT_TRUE(mentions(arch(declarations, "pr(unsigned(x) => u8);"), "Length mismatch for parameter 'x' of 'pr'"));
    EXPECT_TRUE(mentions(arch(declarations, "pr(unsigned(x) => n);"), "Type mismatch for parameter 'x' of 'pr'"));
    EXPECT_TRUE(mentions(arch("procedure pr(signal x : out integer) is begin null; end; ", "pr(std_logic_vector(x) => v4);"),
                         "Cannot convert 'integer' to 'std_logic_vector'"));
}

TEST(Diagnostics_Formals, VariableParametersNeedVariables)
{
    const std::string declarations = "procedure pr(variable x : inout integer) is begin null; end; ";
    EXPECT_TRUE(mentions(arch(declarations, "pr(x => n);"), "must be a variable, because the parameter is declared 'variable'"));
    EXPECT_EQ(proc("pr(x => local);", declarations, "variable local : integer := 0;"), kOk);
    EXPECT_TRUE(mentions(proc("pr(x => 3);", declarations), "must be a variable"));
}

TEST(Diagnostics_Formals, AnInputCannotReceiveAnOutput)
{
    const std::string declarations = "procedure pr(variable x : out integer) is begin x := 1; end; ";
    EXPECT_EQ(proc("pr(x => local);", declarations, "variable local : integer := 0;"), kOk);
    const std::string message = analysisError("entity t is port (i : in integer); end t; architecture r of t is procedure pr(variable x : out integer) is begin x := 1; end; "
                                              "begin process begin pr(x => i); wait; end process; end r;");
    EXPECT_TRUE(mentions(message, "must be a variable")) << message;
}

// ===========================================================================
// 4. ALIASES
// ===========================================================================

TEST(Diagnostics_Aliases, AnAliasNeedsANameOfItsOwn)
{
    EXPECT_TRUE(mentions(arch("signal g : integer; function f(x : integer) return integer is begin return x; end; alias g is f [integer return integer];"),
                         "'g' is already declared in this region"));
}

TEST(Diagnostics_Aliases, WhatCanBeAliased)
{
    EXPECT_TRUE(mentions(arch("component t is end component; alias x is t;"), "already declared"));
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; alias x is keep;"), "is an attribute, not a value"));
    EXPECT_TRUE(mentions(arch("alias x is a'event;"), "An alias must name an object, a type or a subprogram; this expression is none of them"));
    EXPECT_TRUE(mentions(arch("alias x is n'foo;"), "Unknown attribute 'foo'"));
    EXPECT_TRUE(mentions(arch("alias x is zz;"), "'zz' is not declared"));
}

TEST(Diagnostics_Aliases, SubtypesMustAgreeWithWhatTheyName)
{
    EXPECT_TRUE(mentions(arch("signal g : std_logic_vector(3 downto 0); alias x : std_logic_vector(1 downto 0) is g;"),
                         "has 2 element(s), but 'std_logic_vector(3 downto 0)' has 4"));
    EXPECT_TRUE(mentions(arch("signal g : std_logic_vector(3 downto 0); alias x : integer is g;"), "does not match the type"));
    EXPECT_EQ(arch("signal g : std_logic_vector(3 downto 0); alias x : std_logic_vector(3 downto 0) is g;"), kOk);
    EXPECT_EQ(arch("signal g : std_logic_vector(3 downto 0); alias x : std_logic_vector is g;"), kOk);
}

TEST(Diagnostics_Aliases, TypeAliases)
{
    EXPECT_EQ(arch("type t1 is range 0 to 5; alias t2 is t1; signal s : t2;"), kOk);
    EXPECT_TRUE(mentions(arch("type t1 is range 0 to 5; alias t2 : t1 is t1;"), "An alias of a type cannot have a subtype indication"));
    EXPECT_TRUE(mentions(arch("type t1 is range 0 to 5; alias t2 is t1 [t1];"), "An alias of a type cannot have a signature"));
}

TEST(Diagnostics_Aliases, SubprogramAliases)
{
    const std::string one = "function f(x : integer) return integer is begin return x; end; ";
    EXPECT_EQ(arch(one + "alias f2 is f;"), kOk);
    EXPECT_EQ(arch(one + "alias f2 is f [integer return integer];"), kOk);
    EXPECT_EQ(arch(one + "alias \"+\" is f [integer return integer];"), kOk) << "an operator symbol with one parameter is a unary operator";

    EXPECT_TRUE(mentions(arch(one + "function f(x : std_logic) return integer is begin return 1; end; alias f2 is f;"),
                         "'f' is overloaded; the alias needs a signature"));
    EXPECT_TRUE(mentions(arch(one + "alias f2 : integer is f;"), "An alias of a subprogram cannot have a subtype indication"));
    EXPECT_TRUE(mentions(arch(one + "alias f2 is f [integer return integer]; alias f2 is f [integer return integer];"),
                         "is already declared under the name 'f2' in this region"));
    EXPECT_TRUE(mentions(arch("signal g : integer; alias x is g [integer];"), "An alias with a signature must name a subprogram"));
    EXPECT_TRUE(mentions(arch(one + "alias f2 is f [std_logic return integer];"), "No"));
}

// ===========================================================================
// 5. ATTRIBUTE SPECIFICATIONS
// ===========================================================================

TEST(Diagnostics_AttributeSpecifications, EveryEntityClassIsCheckedAgainstItsItem)
{
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of keep : signal is true;"), "'keep' is an attribute, which is not what the specification names (signal)"));
    EXPECT_TRUE(mentions(arch("function f return integer is begin return 1; end; attribute keep : boolean; attribute keep of f : signal is true;"),
                         "'f' is a subprogram, which is not what the specification names (signal)"));
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of n : variable is true;"), "'n' is a signal, which is not what the specification names (variable)"));
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of red : signal is true;"), "'red'"));
    EXPECT_TRUE(mentions(arch("constant k : integer := 3; attribute keep : boolean; attribute keep of k : signal is true;"),
                         "'k' is a constant, which is not what the specification names (signal)"));
    EXPECT_TRUE(mentions(arch("type t1 is range 0 to 5; attribute keep : boolean; attribute keep of t1 : subtype is true;"),
                         "'t1' is a type, which is not what the specification names (subtype)"));
    EXPECT_TRUE(mentions(arch("subtype s1 is integer range 0 to 5; attribute keep : boolean; attribute keep of s1 : type is true;"),
                         "'s1' is a subtype, which is not what the specification names (type)"));
}

TEST(Diagnostics_AttributeSpecifications, ComponentsFunctionsAndProcedures)
{
    const std::string design = "entity leaf is port (i : in std_logic); end leaf; ";
    EXPECT_EQ(analysisError(design + "entity t is end t; architecture r of t is component leaf is port (i : in std_logic); end component; "
                                      "attribute keep : boolean; attribute keep of leaf : component is true; begin end r;"), kOk);
    EXPECT_EQ(arch("function f return integer is begin return 1; end; attribute keep : boolean; attribute keep of f : function is true;"), kOk);
    EXPECT_EQ(arch("procedure pr is begin null; end; attribute keep : boolean; attribute keep of pr : procedure is true;"), kOk);
    EXPECT_TRUE(mentions(arch("procedure pr is begin null; end; attribute keep : boolean; attribute keep of pr : function is true;"), "'pr' is a subprogram"));
}

TEST(Diagnostics_AttributeSpecifications, LiteralsAndUnits)
{
    EXPECT_TRUE(mentions(arch("attribute code : integer; attribute code of red : literal is 1; attribute code of red : literal is 2;"),
                         "The attribute 'code' is already specified for 'red'"));
    EXPECT_EQ(arch("type len is range 0 to 100 units nm; um = 1000 nm; end units; attribute code : integer; attribute code of all : units is 3; "
                   "constant x : integer range 3 to 3 := nm'code; constant y : integer range 3 to 3 := um'code;"), kOk);
    EXPECT_EQ(arch("attribute code : integer; attribute code of all : literal is 3; constant x : integer range 3 to 3 := red'code;"), kOk);
    EXPECT_TRUE(mentions(arch("attribute code : integer; attribute code of blue : literal is 1; constant x : integer := red'code;"), "no value for 'red'"));
    EXPECT_TRUE(mentions(arch("type len is range 0 to 100 units nm; um = 1000 nm; end units; attribute code : integer; attribute code of red : units is 1;"),
                         "'red' is not a physical unit of a type declared in this region"));
}

TEST(Diagnostics_AttributeSpecifications, ListsOfNames)
{
    EXPECT_EQ(arch("attribute keep : boolean; attribute keep of a, b, c : signal is true;"), kOk);
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of a, a : signal is true;"), "is already specified for 'a'"));
    EXPECT_TRUE(mentions(arch("attribute keep : boolean; attribute keep of a, zz : signal is true;"), "'zz' is not declared"));
}

// ===========================================================================
// 6. CHOICES
// ===========================================================================

TEST(Diagnostics_Choices, OthersStandsAlone)
{
    EXPECT_TRUE(mentions(proc("case v4 is when \"0000\" | others => null; end case;"), "'others' must be the only choice of its list"));
    EXPECT_TRUE(mentions(proc("case n is when 1 | others => null; end case;"), "'others' must be the only choice of its list"));
}

TEST(Diagnostics_Choices, ArraySelectorsOverCharacters)
{
    EXPECT_TRUE(mentions(proc("case k is when others => null; end case;", "type ia is array (0 to 1) of integer; signal k : ia;"),
                         "must be an integer, an enumeration or an array of characters"));
    EXPECT_TRUE(mentions(proc("case v4 is when 1 => null; when others => null; end case;"), "must be string literals"));
    EXPECT_TRUE(mentions(proc("case v4 is when \"0000\" => null; when \"0000\" => null; when others => null; end case;"), "The same string literal appears more than once"));
    EXPECT_TRUE(mentions(proc("case v4 is when \"0000\" => null; end case;"), "cannot list every value; end them with an 'others' choice"));
}

TEST(Diagnostics_Choices, MatchingChoices)
{
    EXPECT_EQ(proc("case? v4 is when \"0-0-\" => null; when others => null; end case?;"), kOk);
    EXPECT_EQ(proc("case? a is when '1' => null; when '0' => null; when others => null; end case?;"), kOk);
    EXPECT_TRUE(mentions(proc("case? v4 is when v2 => null; when others => null; end case?;"), "Length mismatch for a choice of the 'case?' statement"));
    EXPECT_TRUE(mentions(proc("case? v4 is when \"0x0-\" => null; when others => null; end case?;"), "not a value of 'std_logic'"));
    EXPECT_TRUE(mentions(proc("case? v4 is when \"0-0-\" | \"0-0-\" => null; when others => null; end case?;"), "can match the same value"));
    EXPECT_TRUE(mentions(proc("case? a is when '1' => null; when 'x' => null; when others => null; end case?;"), "Character literal 'x' is not a value of any visible type"));
    EXPECT_TRUE(mentions(proc("case? n is when 1 => null; when others => null; end case?;"), "must be a std_logic or an array of std_logic to be matched with '?'"));
}

// ===========================================================================
// 7. SCALAR SUBTYPES AND NUMERIC TYPES
// ===========================================================================

TEST(Diagnostics_Subtypes, IntegerRanges)
{
    EXPECT_EQ(arch("subtype s is integer range 0 to 3; subtype u is s range 1 to 2;"), kOk);
    EXPECT_TRUE(mentions(arch("subtype s is integer range 5 to 1;"), "The range '5 to 1' of a subtype of 'integer' is empty"));
    EXPECT_TRUE(mentions(arch("subtype s is natural range -1 to 3;"), "is outside the range of 'integer range 0 to 2147483647'"));
    EXPECT_TRUE(mentions(arch("signal s : std_logic_vector range 0 to 3;"), "is not a scalar type and cannot take a range constraint"));
}

TEST(Diagnostics_Subtypes, RealRanges)
{
    EXPECT_EQ(arch("subtype s is real range 0.0 to 1.0; subtype u is s range 0.25 to 0.5;"), kOk);
    EXPECT_EQ(arch("subtype s is real range 3.0 downto 1.0;"), kOk);
    EXPECT_TRUE(mentions(arch("subtype s is real range 0.0 to 1.0; subtype u is s range 0.0 to 2.0;"), "The range is outside the range of 'real range 0.000000 to 1.000000'"));
    EXPECT_TRUE(mentions(arch("signal r : real; subtype s is real range 0.0 to r;"), "A range constraint needs bounds that are constant at analysis time"));
    EXPECT_TRUE(mentions(arch("subtype s is real range v4'range;"), "A range constraint must look like 'range 0.0 to 1.0'"));
    EXPECT_TRUE(mentions(arch("subtype s is real range 3.0 to 1.0;"), "The range of a subtype of 'real' is empty"));
    EXPECT_TRUE(mentions(arch("subtype s is real range 0 to 1.0;"), "The bounds of a range on 'real' must be real numbers, but found 'universal integer'"));
}

TEST(Diagnostics_Subtypes, PhysicalRanges)
{
    EXPECT_EQ(arch("subtype short is time range 0 ns to 100 ns; signal w : short := 10 ns;"), kOk);
    EXPECT_EQ(arch("subtype short is time range 100 ns downto 0 ns;"), kOk);
    EXPECT_EQ(arch("subtype short is time range 0 ns to 100 ns; subtype shorter is short range 1 ns to 5 ns;"), kOk);
    EXPECT_TRUE(mentions(arch("subtype short is time range 0 ns to 100 ns; signal w : short := 101 ns;"), "outside the range of 'time range"));
    EXPECT_TRUE(mentions(arch("subtype short is time range 5 ns to 1 ns;"), "The range of a subtype of 'time' is empty"));
    EXPECT_TRUE(mentions(arch("subtype short is time range 0 ns to 100 ns; subtype longer is short range 0 ns to 200 ns;"), "The range is outside the range of"));
    EXPECT_TRUE(mentions(arch("subtype short is time range 0 to 100;"), "must be values of that type"));
    EXPECT_TRUE(mentions(arch("subtype short is time range v4'range;"), "A range constraint must look like 'range 0 ns to 10 ns'"));
    EXPECT_TRUE(mentions(arch("subtype short is time range 0 ns to t;"), "constant at analysis time"));
}

TEST(Diagnostics_Subtypes, PhysicalRangesDependingOnAGenericAreKeptAsConstantsOfTheInstance)
{
    EXPECT_EQ(analysisError("entity e is generic (limit : time := 10 ns); end e; architecture r of e is subtype short is time range 0 ns to limit; "
                            "signal w : short; begin end r;"), kOk);
}

TEST(Diagnostics_NumericTypes, TheRangeMustBeWrittenOut)
{
    EXPECT_TRUE(mentions(arch("type rt is range v4'range;"), "A numeric type needs a range such as 'range 0 to 255'"));
    EXPECT_TRUE(mentions(arch("type rt is range 1 to 2.5;"), "must both be integers or both be reals"));
    EXPECT_TRUE(mentions(arch("type rt is array (real range <>) of std_logic;"), "must be an integer or enumeration type, but 'real' is not"));
}

TEST(Diagnostics_Units, UnitsAreCheckedAgainstEachOtherAndTheRegion)
{
    EXPECT_TRUE(mentions(arch("type len is range 0 to 100 units nm; um = 1000 nm; um = 3 nm; end units;"), "Unit 'um' is declared twice in type 'len'"));
    EXPECT_TRUE(mentions(arch("type len is range 0 to 100 units nm; um = 0 nm; end units;"), "Unit 'um' is too large or not positive"));
    EXPECT_TRUE(mentions(arch("type len is range 0 to 100 units nm; um = 9223372036854775807 nm; big = 1000 um; end units;"), "Unit 'big' is too large or not positive"));
    EXPECT_TRUE(mentions(arch("signal nm : integer; type len is range 0 to 100 units nm; end units;"), "Unit 'nm' conflicts with an existing declaration in this region"));
    EXPECT_TRUE(mentions(arch("type len is range 0 to 100 units nm; um = 1000 mm; end units;"), "'mm', which is not an earlier unit of this type"));
    EXPECT_TRUE(mentions(arch("type len is range 0 to 100 units nm; um = 1000 nm; end units; signal um : integer;"), "'um' is already declared in this region as an enumeration literal or a physical unit"));
}

// ===========================================================================
// 8. OPERATORS
// ===========================================================================

TEST(Diagnostics_Operators, ConcatenationOfTwoElementsNeedsAContext)
{
    EXPECT_TRUE(mentions(arch("", "n <= 0; p <= (a & b) = v2;"), "the array type of the result cannot be determined from the context"));
    EXPECT_EQ(proc("v2 <= a & b;"), kOk);
    EXPECT_TRUE(mentions(proc("n <= 1; v4 <= a & b;"), "Length mismatch"));
}

TEST(Diagnostics_Operators, NumericVectors)
{
    EXPECT_EQ(proc("u4 <= u4 / u4; u4 <= u4 mod u4; u4 <= u4 rem u4; u4 <= u4 and u4;"), kOk);
    EXPECT_TRUE(mentions(proc("u4 <= u4 + a;"), "can only be combined with another unsigned or with an integer"));
    EXPECT_TRUE(mentions(proc("u4 <= u4 ** 2;"), "operator '**' is not defined for unsigned"));
    EXPECT_TRUE(mentions(proc("u4 <= u4 xor 2;"), "logical operators work on boolean, std_logic and arrays of them"));
    EXPECT_TRUE(mentions(proc("u4 <= u4 + s4;"), "unsigned and signed values cannot be mixed"));
    EXPECT_TRUE(mentions(proc("u4 <= s4 * s4;"), "expected 'unsigned(3 downto 0)' but the value has type 'signed(7 downto 0)'"));
    EXPECT_TRUE(mentions(proc("u4 <= u4 * u4;"), "Length mismatch"));
    EXPECT_TRUE(mentions(proc("v4 <= u4 / u4;"), "convert the value with a type conversion"));
}

TEST(Diagnostics_Operators, NumericVectorsWithIntegers)
{
    EXPECT_EQ(proc("u4 <= u4 + 1; u4 <= 1 + u4; u4 <= u4 - 1;"), kOk);
    EXPECT_TRUE(mentions(proc("u4 <= u4 and 1;"), "logical operators work on boolean, std_logic and arrays of them"));
}

TEST(Diagnostics_Operators, PhysicalValues)
{
    EXPECT_EQ(proc("t <= 2 * 3 ns; t <= 3 ns * 2; t <= t * 2; t <= t / 2;"), kOk);
    EXPECT_TRUE(mentions(proc("t <= 1 ns * p;"), "arithmetic needs numeric operands"));
    EXPECT_TRUE(mentions(proc("t <= t + n;"), "physical values can only be added to or subtracted from values of the same type"));
    EXPECT_TRUE(mentions(proc("t <= t / t;"), "Type mismatch for signal 't': expected 'time' but the value has type 'universal integer'"));
    EXPECT_TRUE(mentions(proc("n <= t * t;"), "two physical values can only be divided, and only by the same type"));
}

TEST(Diagnostics_Operators, UniversalIntegersAndRealsMix)
{
    EXPECT_EQ(arch("constant r1 : real := 2 * 1.5; constant r2 : real := 2.0 * 1; constant r3 : real := 1.5 * 2;"), kOk);
}

TEST(Diagnostics_Operators, RangesAreNotValues)
{
    EXPECT_TRUE(mentions(proc("n <= (1 to 3);"), "A range such as 'to' is not a value"));
    EXPECT_TRUE(mentions(proc("n <= (3 downto 1);"), "A range such as 'downto' is not a value"));
    EXPECT_TRUE(mentions(proc("n <= n'range;"), "'range can only be used where a range is expected"));
}

TEST(Diagnostics_Conversions, ArraysConvertOnlyToArraysOfTheSameElement)
{
    EXPECT_TRUE(mentions(arch("type ia is array (0 to 3) of integer; signal k : ia;", "v4 <= std_logic_vector(k);"),
                         "Cannot convert 'ia(0 to 3)' to 'std_logic_vector': the types are not closely related"));
    EXPECT_TRUE(mentions(arch("type ia is array (0 to 3) of integer; ", "v4 <= std_logic_vector(w4); n <= ia(v4)(1);"),
                         "Cannot convert 'std_logic_vector(3 downto 0)' to 'ia(0 to 3)': the types are not closely related"));
    EXPECT_TRUE(mentions(arch("", "n <= std_logic_vector(a, b);"), "A conversion to 'std_logic_vector' takes exactly one value"));
    EXPECT_TRUE(mentions(arch("", "n <= std_logic_vector(1 to 2);"), "A conversion to 'std_logic_vector' takes exactly one value"));
}

// ===========================================================================
// 9. CONSTANTS, LOOPS AND STATEMENTS
// ===========================================================================

TEST(Diagnostics_Constants, AnUnconstrainedConstantTakesItsBoundsFromItsValue)
{
    EXPECT_EQ(arch("constant c1 : std_logic_vector := \"0101\"; constant k : integer range 4 to 4 := c1'length;"), kOk);
    EXPECT_TRUE(mentions(arch("function g return std_logic_vector is begin return \"0000\"; end; constant c1 : std_logic_vector := g;"),
                         "Cannot determine the index range of constant 'c1'; give its type an explicit range"));
}

TEST(Diagnostics_Loops, RangesOfEveryShape)
{
    EXPECT_EQ(proc("for i in v4'reverse_range loop null; end loop; for j in natural'reverse_range loop null; end loop;"), kOk);
    EXPECT_EQ(proc("for i in rg loop null; end loop; for j in rg range 0 to 1 loop null; end loop;", "subtype rg is integer range 0 to 3;"), kOk);
    EXPECT_EQ(proc("for i in 0 to 1 loop for j in i to 3 loop null; end loop; end loop;"), kOk);
}

TEST(Diagnostics_Statements, AssignmentTargets)
{
    EXPECT_EQ(arch("", "a <= unaffected;"), kOk);
    EXPECT_EQ(proc("a <= unaffected;"), kOk);
    EXPECT_TRUE(mentions(proc("n := 1;"), "'n' is a signal; assign it with '<=' instead of ':='"));
    EXPECT_TRUE(mentions(proc("for i in 0 to 1 loop i <= 3; end loop;"), "'i' is a loop parameter and cannot be assigned"));
    EXPECT_TRUE(mentions(proc("k <= 3;", "constant k : integer := 3;"), "'k' is a constant and cannot be assigned"));
    EXPECT_TRUE(mentions(proc("for i in 0 to 1 loop i := 3; end loop;"), "'i' is a loop parameter and cannot be assigned"));
    EXPECT_TRUE(mentions(proc("k := 3;", "constant k : integer := 3;"), "'k' is a constant and cannot be assigned"));
    EXPECT_TRUE(mentions(proc("leaf(1);"), "'leaf' is not declared"));
    EXPECT_TRUE(mentions(arch("", "a.b;"), "A procedure call must name the procedure it calls"));
    EXPECT_TRUE(mentions(proc("a.b;"), "A procedure call must name the procedure it calls"));
    EXPECT_TRUE(mentions(proc("f(1) := 1;", "function f(x : integer) return integer is begin return x; end;"), "'f' is not a variable"));
}

TEST(Diagnostics_Statements, ValuesOfEnumerationSubtypesAreNamedInMessages)
{
    EXPECT_TRUE(mentions(arch("subtype rg is color range red to green; constant c : rg := blue;"), "outside the range of 'color range red to green'"));
}

// ===========================================================================
// 10. FUNCTIONS MUST RETURN ON EVERY PATH
// ===========================================================================

namespace
{
    std::string returning(const std::string& body)
    {
        return arch("function f(x : integer) return integer is begin " + body + " end;");
    }
}

TEST(Diagnostics_ReturnPaths, LoopsThatNeverEndNeedNoReturnAfterThem)
{
    EXPECT_EQ(returning("loop if x > 1 then return 1; end if; end loop;"), kOk);
    EXPECT_EQ(returning("loop case x is when 1 => return 1; when others => return 2; end case; end loop;"), kOk);
    EXPECT_EQ(returning("outer : loop inner : loop exit inner; end loop; end loop;"), kOk);
    EXPECT_EQ(returning("loop for i in 0 to 1 loop exit; end loop; end loop;"), kOk);
    EXPECT_EQ(returning("loop while x > 1 loop exit; end loop; end loop;"), kOk);
    EXPECT_EQ(returning("loop loop exit; end loop; end loop;"), kOk);
    EXPECT_EQ(returning("outer : loop loop exit when x = 1; end loop; return 1; end loop;"), kOk);
}

TEST(Diagnostics_ReturnPaths, AnExitLetsControlReachTheEnd)
{
    const std::string message = "can reach its end without a return statement";
    EXPECT_TRUE(mentions(returning("loop if x > 1 then exit; end if; end loop;"), message));
    EXPECT_TRUE(mentions(returning("loop if x > 1 then return 1; else exit; end if; end loop;"), message));
    EXPECT_TRUE(mentions(returning("loop case x is when 1 => exit; when others => null; end case; end loop;"), message));
    EXPECT_TRUE(mentions(returning("outer : loop inner : loop exit outer; end loop; end loop;"), message));
    EXPECT_TRUE(mentions(returning("loop exit when x > 1; end loop;"), message));
    EXPECT_TRUE(mentions(returning("outer : loop for i in 0 to 1 loop exit outer; end loop; end loop;"), message));
    EXPECT_TRUE(mentions(returning("while true loop return 1; end loop;"), message));
}

TEST(Diagnostics_ReturnPaths, BranchesMustAllReturn)
{
    const std::string message = "can reach its end without a return statement";
    EXPECT_EQ(returning("if x > 1 then return 1; elsif x > 2 then return 2; else return 3; end if;"), kOk);
    EXPECT_EQ(returning("case x is when 1 => return 1; when others => return 2; end case;"), kOk);
    EXPECT_TRUE(mentions(returning("if x > 1 then return 1; elsif x > 2 then null; else return 3; end if;"), message));
    EXPECT_TRUE(mentions(returning("case x is when 1 => return 1; when others => null; end case;"), message));
    EXPECT_TRUE(mentions(returning("if x > 1 then return 1; end if;"), message));
}

// ===========================================================================
// 11. THE STRUCTURE OF A DESIGN
// ===========================================================================

TEST(Diagnostics_Design, EntitiesAndArchitectures)
{
    EXPECT_TRUE(mentions(analysisError("entity e is end e; entity e is end e;"), "Entity 'e' is declared twice"));
    EXPECT_TRUE(mentions(analysisError("architecture r of e is begin end r;"), "Architecture 'r' belongs to the unknown entity 'e'"));
    EXPECT_TRUE(mentions(analysisError("entity e is port (a : in std_logic; a : out std_logic); end e;"), "Port 'a' is declared twice in entity 'e'"));
    EXPECT_TRUE(mentions(analysisError("entity e is generic (g : integer; g : integer); end e;"), "'g' is already declared in this region"));
}

TEST(Diagnostics_Design, ComponentsAndInstances)
{
    EXPECT_TRUE(mentions(analysisError("entity e is end e; architecture r of e is component c is end component; begin u : c; end r;"),
                         "Component 'c' has no entity of the same name"));
    EXPECT_TRUE(mentions(analysisError("entity e is end e; architecture r of e is component e is end component; begin u : e; u : e; end r;"),
                         "'u' is already declared in this region"));
    EXPECT_TRUE(mentions(analysisError("entity e is end e; architecture r of e is begin u : nothing; end r;"), "Unknown component 'nothing'"));
}

// ===========================================================================
// 12. SELECTED ASSIGNMENTS AND OVERLOADS INSIDE SUBPROGRAMS AND PROCESSES
// ===========================================================================

TEST(Diagnostics_Subprograms, ASelectedAssignmentIsASignalAssignment)
{
    EXPECT_TRUE(mentions(arch("function f return integer is begin with a select b <= '1' when others; return 1; end;"), "A function cannot assign signals"));
    EXPECT_EQ(arch("procedure pr(signal s : out std_logic) is begin with a select s <= '1' when '0', '0' when others; end;"), kOk);
    EXPECT_TRUE(mentions(arch("procedure pr is begin with a select b <= '1' when others; end;"), "so it can only assign its own signal parameters"));
    EXPECT_EQ(proc("with a select b <= '1' when '0', '0' when others;"), kOk);
}

TEST(Diagnostics_Subprograms, ANonSubprogramInAnOuterRegionDoesNotHideAnInnerFunction)
{
    // The `f` of the process is the only function visible: the signal `f` of the architecture is hidden by it, not overloaded.
    EXPECT_EQ(analysisError("entity e is end e; architecture r of e is signal f : integer; signal n : integer; begin "
                            "process function f(x : integer) return integer is begin return x; end; begin n <= f(1); wait; end process; end r;"), kOk);
}

TEST(Diagnostics_Subprograms, ACallOfAFunctionAsAStatementIsRefused)
{
    EXPECT_TRUE(mentions(analysisError("entity e is end e; architecture r of e is signal f : integer; begin "
                                       "process function f(x : integer) return integer is begin return x; end; begin f(1); wait; end process; end r;"),
                         "'f' is a function, and a function can only be called inside an expression"));
}

TEST(Diagnostics_Aliases, ValuesCanBeAliasedToo)
{
    EXPECT_EQ(arch("alias r is red; constant cc : color := r;"), kOk);
    EXPECT_EQ(arch("type len is range 0 to 100 units nm; um = 1000 nm; end units; alias nano is nm;"), kOk);
    EXPECT_EQ(arch("constant k : integer := 3; alias k2 is k; constant cc : integer range 3 to 3 := k2;"), kOk);
    EXPECT_TRUE(mentions(arch("constant k : integer := 3; alias k2 is k; constant cc : integer range 4 to 4 := k2;"), "outside the range"));
}

// ===========================================================================
// 13. THE LAST CORNERS
// ===========================================================================

TEST(Diagnostics_Corners, AnInputParameterCannotReceiveAnOutput)
{
    const std::string declarations = "procedure pr(variable x : out integer) is begin x := 1; end; "
                                     "procedure outer(variable v : in integer) is begin pr(v); end; ";
    EXPECT_TRUE(mentions(arch(declarations), "is an input and cannot receive the output"));
}

TEST(Diagnostics_Corners, ASliceWhoseBoundsAreNotConstantIsAssociatedWithoutCoverageChecks)
{
    EXPECT_EQ(arch("procedure pr(signal s : out std_logic_vector(3 downto 0)) is begin null; end; ", "pr(s(n downto 0) => v2);"), kOk);
}

TEST(Diagnostics_Corners, AttributeSpecificationsNameWhatTheItemIs)
{
    EXPECT_TRUE(mentions(proc("", "", "variable v : integer; attribute keep : boolean; attribute keep of v : signal is true;"),
                         "'v' is a variable, which is not what the specification names (signal)"));
    EXPECT_TRUE(mentions(analysisError("entity leaf is port (i : in std_logic); end leaf; entity t is end t; architecture r of t is "
                                       "component leaf is port (i : in std_logic); end component; attribute keep : boolean; "
                                       "attribute keep of leaf : signal is true; begin end r;"),
                         "'leaf' is a component, which is not what the specification names (signal)"));
}

TEST(Diagnostics_Corners, AMatchingChoiceMustBeALiteral)
{
    EXPECT_TRUE(mentions(proc("case? v4 is when k4 => null; when others => null; end case?;", "constant k4 : std_logic_vector(3 downto 0) := \"0101\";"),
                         "The choices of the 'case?' statement must be literals of the selector's type"));
}

TEST(Diagnostics_Corners, TheChoicesOfAnAggregateMustBeConstant)
{
    EXPECT_TRUE(mentions(proc("null;", "", "variable w : std_logic_vector(1 downto 0) := (n => '1', others => '0');"), "An aggregate choice must be a static integer expression"));
}

TEST(Diagnostics_Corners, OperatorsThatDoNotFoldStillTypeCheck)
{
    EXPECT_EQ(arch("constant cond : boolean := ?? '1'; constant one : time := ns;"), kOk);
    EXPECT_EQ(arch("constant k4 : std_logic_vector(3 downto 0) := \"0101\"; constant reduced : std_logic := and k4;"), kOk);
    EXPECT_EQ(arch("constant s : real := real(3);"), kOk);
}

TEST(Diagnostics_Corners, AttributesOfASubtypeWhoseRangeDependsOnAGenericHaveNoValue)
{
    EXPECT_EQ(analysisError("entity e is generic (g : integer := 3); end e; architecture r of e is subtype s is integer range 0 to g; "
                            "constant x : integer range 9 to 9 := s'high; begin end r;"), kOk);
    EXPECT_EQ(analysisError("entity e is generic (g : integer := 3); end e; architecture r of e is subtype s is integer range 0 to g; "
                            "begin process begin for i in s loop null; end loop; wait; end process; end r;"), kOk);
    EXPECT_EQ(analysisError("entity e is generic (g : real := 3.0); end e; architecture r of e is subtype s is real range 0.0 to g; "
                            "signal x : s; begin end r;"), kOk);
}

TEST(Diagnostics_Corners, TypesAndComponentsAreNotValues)
{
    EXPECT_TRUE(mentions(proc("n <= integer;"), "'integer' is a type, not a value"));
    EXPECT_TRUE(mentions(analysisError("entity leaf is end leaf; entity t is end t; architecture r of t is component leaf is end component; "
                                       "signal n : integer; begin n <= leaf; end r;"),
                         "'leaf' is a component, not a value"));
    EXPECT_TRUE(mentions(proc("n <= red;"), "Type mismatch"));
}

TEST(Diagnostics_Corners, TwoNumericVectorsHaveNoPower)
{
    EXPECT_TRUE(mentions(proc("u4 <= u4 ** u4;"), "operator '**' is not defined for unsigned"));
}

TEST(Diagnostics_Corners, ExitsInsideNestedLoopsOfEveryKind)
{
    const std::string message = "can reach its end without a return statement";
    EXPECT_TRUE(mentions(returning("outer : loop loop exit outer; end loop; end loop;"), message));
    EXPECT_TRUE(mentions(returning("outer : loop while x > 1 loop exit outer; end loop; end loop;"), message));
    EXPECT_TRUE(mentions(returning("outer : loop for i in 0 to 1 loop exit outer; end loop; end loop;"), message));
    EXPECT_TRUE(mentions(returning("outer : loop for i in 0 to 1 loop if x > 1 then exit outer; end if; end loop; end loop;"), message));
}

TEST(Diagnostics_Corners, AVariableTargetMustBeAName)
{
    EXPECT_TRUE(mentions(proc("v4'length := 3;"), "must be a variable"));
}

TEST(Diagnostics_Corners, ASignalTargetMustBeAnObject)
{
    EXPECT_TRUE(mentions(arch("function f return integer is begin return 1; end;", "f <= 1;"), "The target of a signal assignment must be a signal or a port, but 'f' is not an object"));
}

TEST(Diagnostics_Corners, AConditionalValueMayOmitItsLastElse)
{
    EXPECT_EQ(arch("", "a <= '1' when b = '1';"), kOk);
    EXPECT_EQ(arch("", "(a, b) <= v2 when c = '1';"), kOk);
    EXPECT_EQ(proc("a <= '1' when b = '1';"), kOk);
    EXPECT_EQ(proc("w := 1 when a = '1';", "", "variable w : integer;"), kOk) << "the variable keeps its value when no condition holds";
}
