// vhdl2008.test.cc — VHDL-2008 forms: concurrent assertions, conditional values in variable assignments, `unaffected`,
// matching relational operators, `??`, logical reductions, `case?` / `select?`, and aggregate assignment targets.

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

    std::string proc(const std::string& statements, const std::string& processDeclarations = "", const std::string& declarations = "")
    {
        return arch(declarations, "process " + processDeclarations + " begin " + statements + " wait; end process;");
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }
}

// ===========================================================================
// 1. CONCURRENT ASSERTIONS
// ===========================================================================

TEST(Vhdl2008_Assert, ConcurrentAssertionInAnArchitecture)
{
    EXPECT_EQ(arch("", "assert a = '1';"), kOk);
    EXPECT_EQ(arch("", "check : assert n > 0 and p severity failure;"), kOk);
    EXPECT_EQ(arch(TestUtil::stringTypes(), "assert a = '1' report \"a must be high\" severity warning;"), kOk);
}

TEST(Vhdl2008_Assert, TheConditionMustBeBooleanOrStdLogic)
{
    EXPECT_TRUE(mentions(arch("", "assert n;"), "must be boolean"));
    EXPECT_EQ(arch("", "assert a;"), kOk) << "std_logic converts through the condition operator";
    EXPECT_TRUE(mentions(arch("", "assert v4;"), "must be boolean"));
}

TEST(Vhdl2008_Assert, MessageAndSeverityAreChecked)
{
    const std::string strings = TestUtil::stringTypes();
    EXPECT_TRUE(mentions(arch(strings, "assert p report n;"), "Type mismatch"));
    EXPECT_TRUE(mentions(arch(strings, "assert p report \"x\" severity n;"), "severity_level"));
    EXPECT_TRUE(mentions(arch("", "assert p report \"x\";"), "the type 'string' is not declared"));
    EXPECT_TRUE(mentions(arch(strings, "assert p report \"caf\xc3\xa9\";"), "not a value of 'character'"))
        << "a character the declared type does not list";
}

TEST(Vhdl2008_Assert, ALabelIsDeclared)
{
    EXPECT_TRUE(mentions(arch("", "chk : assert p; chk : assert q;"), "'chk' is already declared"));
    EXPECT_TRUE(mentions(arch("signal chk : integer; ", "chk : assert p;"), "already declared"));
}

TEST(Vhdl2008_Assert, ReadingSignalsIsFine)
{
    EXPECT_EQ(arch("", "assert v4 = w4 or a = '0';"), kOk);
}

// ===========================================================================
// 2. CONDITIONAL AND SELECTED VALUES
// ===========================================================================

TEST(Vhdl2008_Values, ConditionalVariableAssignments)
{
    EXPECT_EQ(proc("k := 1 when a = '1' else 2; k := 1 when p else 2 when q else 3; k := 5 when p;", "variable k : integer;"), kOk);
}

TEST(Vhdl2008_Values, EveryValueAndConditionIsChecked)
{
    EXPECT_TRUE(mentions(proc("k := '1' when p else 2;", "variable k : integer;"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc("k := 1 when p else '0';", "variable k : integer;"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc("k := 1 when n else 2;", "variable k : integer;"), "must be boolean"));
    EXPECT_TRUE(mentions(proc("k := 1 when p else 2 when n else 3;", "variable k : integer;"), "must be boolean"));
}

TEST(Vhdl2008_Values, ConditionalSignalAssignments)
{
    EXPECT_EQ(arch("", "a <= '1' when b = '0' else '0' when c = '1' else 'Z';"), kOk);
    EXPECT_EQ(arch("", "v4 <= \"0000\" when a = '1' else (others => '1');"), kOk);
    EXPECT_TRUE(mentions(arch("", "a <= '1' when n else '0';"), "must be boolean"));
    EXPECT_TRUE(mentions(arch("", "v4 <= \"00\" when a = '1' else \"1111\";"), "element"));
}

TEST(Vhdl2008_Values, ANoElseChainIsFine)
{
    EXPECT_EQ(proc("a <= '1' when b = '1';"), kOk);
}

TEST(Vhdl2008_Values, VeryLongConditionalChains)
{
    std::string chain = "n <= 0";
    for (int i = 1; i < 1500; ++i)
        chain += " when p else " + std::to_string(i);
    chain += ";";
    EXPECT_EQ(arch("", chain), kOk) << "the chain is walked, not recursed into";
}

TEST(Vhdl2008_Values, UnaffectedIsASignalValue)
{
    EXPECT_EQ(arch("", "a <= unaffected;"), kOk);
    EXPECT_EQ(arch("", "a <= '1' when b = '1' else unaffected;"), kOk);
    EXPECT_EQ(arch("", "a <= unaffected when b = '1' else '0';"), kOk);
    EXPECT_EQ(arch("", "with n select a <= '1' when 0, unaffected when others;"), kOk);
    EXPECT_EQ(proc("a <= unaffected;"), kOk);
}

TEST(Vhdl2008_Values, UnaffectedIsNotAVariableValue)
{
    EXPECT_TRUE(mentions(proc("k := unaffected;", "variable k : integer;"), "'unaffected' can only be the value of a signal assignment"));
    EXPECT_TRUE(mentions(proc("k := 1 when p else unaffected;", "variable k : integer;"), "'unaffected' can only be the value of a signal assignment"));
}

TEST(Vhdl2008_Values, UnaffectedInsideAnExpressionIsAnError)
{
    // The grammar only allows `unaffected` as a whole value, so the parser already refuses it inside an expression.
    EXPECT_TRUE(mentions(arch("", "n <= 1 + unaffected;"), "Expected an expression, but found 'unaffected'"));
    EXPECT_TRUE(mentions(arch("", "a <= not unaffected;"), "Expected an expression, but found 'unaffected'"));
}

// ===========================================================================
// 3. MATCHING RELATIONAL OPERATORS
// ===========================================================================

TEST(Vhdl2008_Matching, StdLogicScalars)
{
    EXPECT_EQ(proc("c <= a ?= b; c <= a ?/= b; c <= a ?< b; c <= a ?<= b; c <= a ?> b; c <= a ?>= b; c <= a ?= '-';"), kOk);
}

TEST(Vhdl2008_Matching, TheResultIsStdLogic)
{
    EXPECT_TRUE(mentions(proc("p <= a ?= b;"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc("n <= a ?= b;"), "Type mismatch"));
    EXPECT_EQ(proc("if (a ?= b) then null; end if;"), kOk) << "a std_logic condition converts through the condition operator";
    EXPECT_EQ(proc("if a ?= b and c ?= d then null; end if;"), kOk);
}

TEST(Vhdl2008_Matching, EqualityOnVectorsOfTheSameTypeAndLength)
{
    EXPECT_EQ(proc("a <= v4 ?= w4; a <= v4 ?/= \"1-0-\"; a <= v4 ?= (others => '-');"), kOk);
    EXPECT_TRUE(mentions(proc("a <= v4 ?= v2;"), "must have the same length (4 and 2)"));
    EXPECT_TRUE(mentions(proc("a <= v4 ?= \"00\";"), "element"));
    EXPECT_TRUE(mentions(proc("a <= v4 ?= u4;"), "can only be matched with the same type or with an integer"));
    EXPECT_TRUE(mentions(proc("a <= v4 ?= col;"), "matching equality works on"));
}

TEST(Vhdl2008_Matching, OrderingIsForScalarsAndNumericVectors)
{
    EXPECT_EQ(proc("a <= u4 ?< x4; a <= u4 ?>= 3; a <= 3 ?<= u4; a <= s4 ?> s4;"), kOk);
    EXPECT_TRUE(mentions(proc("a <= v4 ?< w4;"), "matching ordering works on"));
    EXPECT_TRUE(mentions(proc("a <= u4 ?< s4;"), "can only be matched with the same type or with an integer"));
    EXPECT_TRUE(mentions(proc("a <= u4 ?< '1';"), "can only be matched with the same type or with an integer"));
}

TEST(Vhdl2008_Matching, OtherTypesAreRejected)
{
    EXPECT_TRUE(mentions(proc("a <= n ?= m;"), "matching equality works on"));
    EXPECT_TRUE(mentions(proc("a <= col ?= col;"), "matching equality works on"));
    EXPECT_TRUE(mentions(proc("a <= p ?= q;"), "matching equality works on"));
}

// ===========================================================================
// 4. THE CONDITION OPERATOR AND REDUCTIONS
// ===========================================================================

TEST(Vhdl2008_Condition, ConvertsStdLogicToBoolean)
{
    EXPECT_EQ(proc("p <= ?? a; p <= (?? a) and (?? b);"), kOk);
    EXPECT_TRUE(mentions(proc("p <= ?? a and ?? b;"), "Expected ';', but found 'and'")) << "?? is a level of its own: it takes one primary";
    EXPECT_EQ(proc("if ?? a then null; end if; while ?? a loop exit; end loop;"), kOk);
}

TEST(Vhdl2008_Condition, TheOperandIsAStdLogicOrABoolean)
{
    EXPECT_EQ(proc("p <= ?? p;"), kOk);
    EXPECT_TRUE(mentions(proc("p <= ?? n;"), "the condition operator takes a std_logic or a boolean"));
    EXPECT_TRUE(mentions(proc("p <= ?? v4;"), "the condition operator takes a std_logic or a boolean"));
    EXPECT_TRUE(mentions(proc("a <= ?? a;"), "Type mismatch")) << "the result is a boolean";
}

TEST(Vhdl2008_Reductions, ReduceAVectorToItsElementType)
{
    EXPECT_EQ(proc("a <= and v4; a <= or v4; a <= nand v4; a <= nor v4; a <= xor v4; a <= xnor v4; a <= and u4; a <= xor s4;"), kOk);
    EXPECT_EQ(proc("a <= (and v4) or (or w4);"), kOk);
}

TEST(Vhdl2008_Reductions, OnlyLogicalVectorsCanBeReduced)
{
    EXPECT_TRUE(mentions(proc("a <= and a;"), "a logical reduction works on a one-dimensional array of boolean or std_logic"));
    EXPECT_TRUE(mentions(proc("p <= and n;"), "a logical reduction works on"));
    EXPECT_TRUE(mentions(proc("p <= or col;"), "a logical reduction works on"));
}

TEST(Vhdl2008_Reductions, TheResultTypeIsTheElementType)
{
    EXPECT_TRUE(mentions(proc("p <= and v4;"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc("n <= or v4;"), "Type mismatch"));
}

// ===========================================================================
// 5. MATCHING CASE AND SELECT
// ===========================================================================

TEST(Vhdl2008_MatchingCase, ScalarSelectors)
{
    EXPECT_EQ(proc("case? a is when '1' => null; when '0' => null; end case?;"), kOk);
    EXPECT_EQ(proc("case? a is when '-' => null; end case?;"), kOk) << "'-' matches every value";
    EXPECT_EQ(proc("case? a is when '1' => null; when others => null; end case?;"), kOk);
    EXPECT_EQ(proc("case? a is when '0' | '1' => null; end case?;"), kOk);
}

TEST(Vhdl2008_MatchingCase, VectorSelectorsWithDontCares)
{
    EXPECT_EQ(proc("case? v2 is when \"1-\" => null; when \"01\" => null; when \"00\" => null; end case?;"), kOk);
    EXPECT_EQ(proc("case? v2 is when \"--\" => null; end case?;"), kOk);
    EXPECT_EQ(proc("case? v4 is when \"1---\" => null; when \"0---\" => null; end case?;"), kOk);
    EXPECT_EQ(proc("case? v2 is when \"1-\" => null; when others => null; end case?;"), kOk);
}

TEST(Vhdl2008_MatchingCase, ChoicesThatOverlapAreRejected)
{
    const std::string message = proc("case? v2 is when \"1-\" => null; when \"11\" => null; when others => null; end case?;");
    EXPECT_TRUE(mentions(message, "can match the same value")) << message;
    EXPECT_TRUE(mentions(proc("case? v2 is when \"-1\" => null; when \"1-\" => null; when others => null; end case?;"), "can match the same value"));
    EXPECT_TRUE(mentions(proc("case? a is when '-' => null; when '1' => null; end case?;"), "can match the same value"));
}

TEST(Vhdl2008_MatchingCase, EveryValueMustBeCovered)
{
    const std::string message = proc("case? v2 is when \"1-\" => null; when \"01\" => null; end case?;");
    EXPECT_TRUE(mentions(message, "do not cover every value")) << message;
    EXPECT_TRUE(mentions(proc("case? a is when '1' => null; end case?;"), "do not cover every value"));
    EXPECT_EQ(proc("case? v8 is when \"--------\" => null; end case?;"), kOk) << "eight dashes match all 256 values";
    EXPECT_TRUE(mentions(proc("case? v8 is when \"-------0\" => null; end case?;"), "do not cover every value"));
}

TEST(Vhdl2008_MatchingCase, PatternsUseZeroOneAndDash)
{
    EXPECT_TRUE(mentions(proc("case? v2 is when \"1X\" => null; when others => null; end case?;"), "can only contain '0', '1' and '-'"));
    EXPECT_TRUE(mentions(proc("case? a is when 'Z' => null; when others => null; end case?;"), "can only contain '0', '1' and '-'"));
}

TEST(Vhdl2008_MatchingCase, TheSelectorMustBeStdLogicBased)
{
    EXPECT_TRUE(mentions(proc("case? n is when 0 => null; when others => null; end case?;"), "must be a std_logic or an array of std_logic"));
    EXPECT_TRUE(mentions(proc("case? col is when red => null; when others => null; end case?;"), "must be a std_logic or an array of std_logic"));
    EXPECT_EQ(proc("case? u4 is when \"0000\" => null; when others => null; end case?;"), kOk) << "unsigned is an array of std_logic";
}

TEST(Vhdl2008_MatchingCase, ChoicesAreTypeChecked)
{
    EXPECT_TRUE(mentions(proc("case? v2 is when \"1\" => null; when others => null; end case?;"), "element"));
    EXPECT_TRUE(mentions(proc("case? v2 is when 1 => null; when others => null; end case?;"), "cannot be used where") ||
                mentions(proc("case? v2 is when 1 => null; when others => null; end case?;"), "Type mismatch"));
}

TEST(Vhdl2008_MatchingCase, OthersMustBeLast)
{
    EXPECT_TRUE(mentions(proc("case? a is when others => null; when '1' => null; end case?;"), "'others' must be the last choice"));
}

TEST(Vhdl2008_MatchingCase, WideVectorsNeedOthers)
{
    std::string pattern(70, '-');
    const std::string declarations = "signal wide : std_logic_vector(69 downto 0); ";
    EXPECT_TRUE(mentions(arch(declarations, "process begin case? wide is when \"" + pattern + "\" => null; end case?; wait; end process;"),
                         "end them with an 'others' choice"));
    EXPECT_EQ(arch(declarations, "process begin case? wide is when \"" + pattern + "\" => null; when others => null; end case?; wait; end process;"), kOk);
}

TEST(Vhdl2008_MatchingSelect, SelectedAssignmentsMatch)
{
    EXPECT_EQ(arch("", "with v2 select? a <= '1' when \"1-\", '0' when \"01\", 'Z' when \"00\";"), kOk);
    EXPECT_EQ(arch("", "with a select? b <= '1' when '1', '0' when others;"), kOk);
    EXPECT_EQ(proc("with v2 select? a <= '1' when \"1-\", '0' when others;"), kOk);
}

TEST(Vhdl2008_MatchingSelect, TheSameRulesAsCase)
{
    EXPECT_TRUE(mentions(arch("", "with v2 select? a <= '1' when \"1-\", '0' when \"11\", '0' when others;"), "can match the same value"));
    EXPECT_TRUE(mentions(arch("", "with v2 select? a <= '1' when \"1-\";"), "do not cover every value"));
    EXPECT_TRUE(mentions(arch("", "with n select? a <= '1' when 0, '0' when others;"), "must be a std_logic or an array of std_logic"));
    EXPECT_TRUE(mentions(arch("", "with v2 select? a <= n when \"11\", '0' when others;"), "Type mismatch"));
}

TEST(Vhdl2008_MatchingSelect, ThePlainFormsTreatDashAsAnOrdinaryCharacter)
{
    EXPECT_EQ(arch("", "with v2 select a <= '1' when \"1-\", '0' when others;"), kOk);
    EXPECT_TRUE(mentions(arch("", "with v2 select a <= '1' when \"1-\", '0' when \"11\";"), "cannot list every value"))
        << "a plain select over a vector still needs others";
}

// ===========================================================================
// 6. AGGREGATE TARGETS
// ===========================================================================

TEST(Vhdl2008_AggregateTargets, SignalsAreAssignedElementByElement)
{
    EXPECT_EQ(arch("", "(a, b) <= v2;"), kOk);
    EXPECT_EQ(arch("", "(a, b, c, d) <= v4;"), kOk);
    EXPECT_EQ(proc("(a, b) <= v2;"), kOk);
}

TEST(Vhdl2008_AggregateTargets, VariablesAreAssignedToo)
{
    EXPECT_EQ(proc("(x, y) := v2;", "variable x, y : std_logic;"), kOk);
}

TEST(Vhdl2008_AggregateTargets, ElementsAndValueMustAgree)
{
    EXPECT_TRUE(mentions(arch("", "(a, b, c) <= v2;"), "has 3 element(s), but the value"));
    EXPECT_TRUE(mentions(arch("", "(a, n) <= v2;"), "must all have the same type"));
    EXPECT_TRUE(mentions(arch("", "(a, b) <= n;"), "must be an array of 'std_logic'"));
    EXPECT_TRUE(mentions(arch("", "(n, m) <= v2;"), "must be an array of 'integer'"));
}

TEST(Vhdl2008_AggregateTargets, ElementsMustBeWritable)
{
    EXPECT_TRUE(mentions(analysisError("entity t is port (i : in std_logic; o : out std_logic); end t; architecture r of t is begin (i, o) <= \"10\"; end r;"),
                         "Port 'i' is an input and cannot be assigned"));
    EXPECT_TRUE(mentions(proc("(a, k) := v2;", "variable k : std_logic;"), "must be a variable"));
    EXPECT_TRUE(mentions(arch("constant k : std_logic := '1'; ", "(a, k) <= v2;"), "constant and cannot be assigned"));
}

TEST(Vhdl2008_AggregateTargets, NamedElementsAreNotSupported)
{
    EXPECT_TRUE(mentions(arch("", "(0 => a, 1 => b) <= v2;"), "Named elements are not supported in an aggregate target"));
}

TEST(Vhdl2008_AggregateTargets, EachElementIsADriver)
{
    const std::string integers = "type pair is array (0 to 1) of integer; signal both : pair; ";
    EXPECT_EQ(arch(integers, "process begin (n, m) <= both; wait; end process;"), kOk);

    const std::string message = arch(integers, "process begin (n, m) <= both; wait; end process; process begin n <= 1; wait; end process;");
    EXPECT_TRUE(mentions(message, "Signal 'n' has the type 'integer', which cannot have several drivers")) << message;

    EXPECT_EQ(arch("", "process begin (a, b) <= v2; wait; end process; process begin a <= '1'; wait; end process;"), kOk) << "std_logic is resolved";
}

TEST(Vhdl2008_AggregateTargets, ALiteralValueHasNoTypeToTakeFrom)
{
    EXPECT_TRUE(mentions(arch("", "(a, b) <= \"01\";"), "The type of this string literal cannot be determined from its context"));
}

TEST(Vhdl2008_AggregateTargets, ConditionalValuesAreCheckedBranchByBranch)
{
    const std::string other = "signal w2 : std_logic_vector(1 downto 0); ";
    EXPECT_EQ(arch(other, "(a, b) <= v2 when c = '1' else w2;"), kOk);
    EXPECT_EQ(arch(other, "(a, b) <= v2 when c = '1' else w2 when d = '1' else v2;"), kOk);
    EXPECT_EQ(arch(other, "(a, b) <= v2 when c = '1' else unaffected;"), kOk);
    EXPECT_EQ(arch("", "(a, b) <= unaffected;"), kOk);
    EXPECT_EQ(proc("(a, b) <= v2 when c = '1' else v2;"), kOk);

    EXPECT_TRUE(mentions(arch(other, "(a, b) <= v2 when c = '1' else n;"), "must be an array of 'std_logic'"));
    EXPECT_TRUE(mentions(arch(other, "(a, b) <= v4 when c = '1' else v2;"), "has 2 element(s), but the value"));
    EXPECT_TRUE(mentions(arch(other, "(a, b) <= v2 when n else w2;"), "'when' clause"));
}

TEST(Vhdl2008_AggregateTargets, VariablesTakeConditionalValuesButNeverUnaffected)
{
    EXPECT_EQ(proc("(x, y) := v2 when c = '1' else v2;", "variable x, y : std_logic;"), kOk);
    EXPECT_TRUE(mentions(proc("(x, y) := unaffected;", "variable x, y : std_logic;"), "'unaffected' can only be the value of a signal assignment"));
    EXPECT_TRUE(mentions(proc("(x, y) := v2 when c = '1' else unaffected;", "variable x, y : std_logic;"), "'unaffected' can only be the value of a signal assignment"));
}

TEST(Vhdl2008_AggregateTargets, NotInSelectedAssignments)
{
    EXPECT_TRUE(mentions(arch("", "with n select (a, b) <= v2 when others;"), "aggregate target is not supported in a selected"));
}
