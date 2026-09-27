// subprograms.test.cc — functions and procedures: declarations and bodies, parameters, calls, overload
// resolution, operator overloading, return rules, purity and the wait rules. Every rejected program asserts a fragment of
// its message.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    const std::string kSignals =
        "signal a, b, c : std_logic; signal n, m : integer; signal p : boolean; "
        "signal v4 : std_logic_vector(3 downto 0); signal u4 : unsigned(3 downto 0); "
        "type color is (red, green, blue); signal col : color; ";

    std::string arch(const std::string& declarations, const std::string& body = "")
    {
        return analysisError(inArchitecture(kSignals + declarations, body));
    }

    /// `statements` run in one process; `processDeclarations` are its declarative part.
    std::string proc(const std::string& declarations, const std::string& statements, const std::string& processDeclarations = "")
    {
        return arch(declarations, "process " + processDeclarations + " begin " + statements + " wait; end process;");
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    const std::string kInc = "function inc(x : integer) return integer is begin return x + 1; end function inc; ";
}

// ===========================================================================
// 1. DECLARATIONS AND BODIES
// ===========================================================================

TEST(Subprograms_Declarations, FunctionWithBodyCanBeCalled)
{
    EXPECT_EQ(proc(kInc, "n <= inc(m);"), kOk);
    EXPECT_EQ(proc(kInc, "n <= inc(inc(1)) + inc(m);"), kOk);
}

TEST(Subprograms_Declarations, ADeclarationIsCompletedByALaterBody)
{
    EXPECT_EQ(proc("function inc(x : integer) return integer; " + kInc, "n <= inc(1);"), kOk);
    EXPECT_EQ(arch("procedure hit; procedure hit is begin null; end;", "process begin hit; wait; end process;"), kOk);
}

TEST(Subprograms_Declarations, ADeclarationWithoutABodyIsRejected)
{
    const std::string message = arch("function inc(x : integer) return integer;");
    EXPECT_TRUE(mentions(message, "declared but has no body")) << message;
    EXPECT_TRUE(mentions(arch("procedure hit;"), "declared but has no body"));
    EXPECT_TRUE(mentions(proc("", "wait;", "procedure hit; "), "declared but has no body"));
}

TEST(Subprograms_Declarations, BodyCompletingADeclarationMustBeInTheSameRegion)
{
    // The body sits in the process; the declaration is in the architecture, so the region never gets its body.
    const std::string message = arch("procedure hit;", "process procedure hit is begin null; end; begin wait; end process;");
    EXPECT_TRUE(mentions(message, "declared but has no body")) << message;
}

TEST(Subprograms_Declarations, TwoBodiesOrTwoDeclarationsOfOneProfileAreRejected)
{
    EXPECT_TRUE(mentions(arch(kInc + kInc), "already has a body"));
    EXPECT_TRUE(mentions(arch("function f return integer; function f return integer; "), "already declared"));
    EXPECT_TRUE(mentions(arch("procedure hit is begin null; end; procedure hit is begin null; end;"), "already has a body"));
}

TEST(Subprograms_Declarations, ParameterNamesDoNotMakeAnotherProfile)
{
    EXPECT_TRUE(mentions(arch("function f(x : integer) return integer; function f(y : integer) return integer; "), "already declared"));
    EXPECT_TRUE(mentions(arch("function f(x : integer) return integer; function f(y : natural) return integer; "), "already declared"))
        << "a subtype is the same type";
}

TEST(Subprograms_Declarations, PurityOfABodyMustMatchItsDeclaration)
{
    const std::string message = arch("impure function f return integer; function f return integer is begin return 1; end;");
    EXPECT_TRUE(mentions(message, "pure, but it was declared impure")) << message;
}

TEST(Subprograms_Declarations, NameCannotClashWithAnotherKindOfDeclaration)
{
    EXPECT_TRUE(mentions(arch("function n return integer is begin return 1; end;"), "already declared"));
    EXPECT_TRUE(mentions(arch("signal inc : integer; " + kInc), "already declared"));
}

TEST(Subprograms_Declarations, SubprogramsInProcessesAndOtherSubprograms)
{
    EXPECT_EQ(proc("", "n <= twice(2);", "function twice(x : integer) return integer is begin return x * 2; end;"), kOk);
    EXPECT_EQ(arch("function outer(x : integer) return integer is "
                   "  function inner(y : integer) return integer is begin return y + 1; end; "
                   "begin return inner(x) * 2; end;",
                   "process begin n <= outer(3); wait; end process;"), kOk);
}

TEST(Subprograms_Declarations, InnerSubprogramsAreNotVisibleOutside)
{
    const std::string message = arch("function outer return integer is function inner return integer is begin return 1; end; begin return 1; end;",
                                     "process begin n <= inner; wait; end process;");
    EXPECT_TRUE(mentions(message, "'inner' is not declared")) << message;
}

// ===========================================================================
// 2. PARAMETERS
// ===========================================================================

TEST(Subprograms_Parameters, ClassesAndModes)
{
    EXPECT_EQ(arch("procedure p1(constant k : in integer; signal s : out std_logic; variable v : inout integer; x : in bit_like) is begin null; end;"),
              "Unknown type 'bit_like'");
    EXPECT_EQ(arch("procedure p1(constant k : in integer; signal s : out std_logic; variable v : inout integer; x : in integer) is begin null; end;"), kOk);
    EXPECT_EQ(arch("function f(constant k : integer; signal s : std_logic) return integer is begin return k; end;"), kOk);
}

TEST(Subprograms_Parameters, DefaultsOfClassAndMode)
{
    // A parameter of mode `in` is a constant; one of another mode is a variable.
    EXPECT_EQ(arch("procedure bump(x : in integer; y : out integer) is begin y := x + 1; end;",
                   "process variable r : integer; begin bump(1, r); wait; end process;"), kOk);
}

TEST(Subprograms_Parameters, FunctionParametersMustBeInputs)
{
    EXPECT_TRUE(mentions(arch("function f(x : out integer) return integer is begin return 1; end;"), "must have mode 'in'"));
    EXPECT_TRUE(mentions(arch("function f(x : inout integer) return integer is begin return 1; end;"), "must have mode 'in'"));
    EXPECT_TRUE(mentions(arch("function f(variable x : integer) return integer is begin return 1; end;"), "cannot be a variable"));
}

TEST(Subprograms_Parameters, ConstantParametersAreInputs)
{
    EXPECT_TRUE(mentions(arch("procedure p1(constant x : out integer) is begin null; end;"), "must have mode 'in'"));
}

TEST(Subprograms_Parameters, DuplicateParametersAreRejected)
{
    EXPECT_TRUE(mentions(arch("function f(x : integer; x : integer) return integer is begin return 1; end;"), "Parameter 'x' is declared twice"));
}

TEST(Subprograms_Parameters, DefaultValues)
{
    const std::string f = "function f(x : integer := 5) return integer is begin return x; end; ";
    EXPECT_EQ(proc(f, "n <= f; n <= f(1); n <= f(x => 2);"), kOk);
    EXPECT_TRUE(mentions(arch("function f(x : integer := n) return integer is begin return x; end;"), "must be constant"));
    EXPECT_TRUE(mentions(arch("function f(x : integer := '1') return integer is begin return x; end;"), "Type mismatch"));
    EXPECT_TRUE(mentions(arch("procedure p1(x : out integer := 1) is begin null; end;"), "Only a parameter of mode 'in' can have a default"));
}

TEST(Subprograms_Parameters, ParameterTypesMustExist)
{
    EXPECT_TRUE(mentions(arch("function f(x : nothing) return integer is begin return 1; end;"), "Unknown type 'nothing'"));
    EXPECT_TRUE(mentions(arch("function f(x : integer) return nothing is begin return 1; end;"), "Unknown type 'nothing'"));
}

TEST(Subprograms_Parameters, ParametersAreScopedToTheBody)
{
    EXPECT_TRUE(mentions(arch(kInc, "process begin n <= x; wait; end process;"), "'x' is not declared"));
}

TEST(Subprograms_Parameters, InputParametersCannotBeAssigned)
{
    EXPECT_TRUE(mentions(arch("procedure p1(x : in integer) is begin x := 1; end;"), "cannot be assigned"));
    EXPECT_TRUE(mentions(arch("procedure p1(signal s : in std_logic) is begin s <= '1'; end;"), "is an input and cannot be assigned"));
    EXPECT_TRUE(mentions(arch("procedure p1(variable v : in integer) is begin v := 1; end;"), "input parameter"));
}

TEST(Subprograms_Parameters, OutputParametersCanBeAssignedAndRead)
{
    EXPECT_EQ(arch("procedure p1(y : out integer) is begin y := 1; y := y + 1; end;"), kOk);
    EXPECT_EQ(arch("procedure p1(signal s : out std_logic) is begin s <= '1'; end;"), kOk);
}

// ===========================================================================
// 3. CALLS
// ===========================================================================

TEST(Subprograms_Calls, NamedPositionalAndMixedAssociations)
{
    const std::string f = "function f(x : integer; y : integer := 0; z : integer := 0) return integer is begin return x + y + z; end; ";
    EXPECT_EQ(proc(f, "n <= f(1, 2, 3); n <= f(1, z => 3); n <= f(z => 1, x => 2); n <= f(x => 1);"), kOk);
}

TEST(Subprograms_Calls, AssociationErrors)
{
    const std::string f = "function f(x : integer; y : integer) return integer is begin return x + y; end; ";
    EXPECT_TRUE(mentions(proc(f, "n <= f(1);"), "not associated and has no default"));
    EXPECT_TRUE(mentions(proc(f, "n <= f(1, 2, 3);"), "Too many associations"));
    EXPECT_TRUE(mentions(proc(f, "n <= f(x => 1, 2);"), "positional association cannot follow"));
    EXPECT_TRUE(mentions(proc(f, "n <= f(x => 1, y => 2, x => 3);"), "connected twice"));
    EXPECT_TRUE(mentions(proc(f, "n <= f(x => 1, w => 2);"), "has no parameter named 'w'"));
    EXPECT_TRUE(mentions(proc(f, "n <= f(x => 1, y => open);"), "cannot be left open"));
}

TEST(Subprograms_Calls, ArgumentsAreTypeChecked)
{
    EXPECT_TRUE(mentions(proc(kInc, "n <= inc('1');"), "Type mismatch for parameter 'x' of 'inc'"));
    EXPECT_TRUE(mentions(proc(kInc, "n <= inc(v4);"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc(kInc, "n <= inc(m + '1');"), "cannot be applied"));
}

TEST(Subprograms_Calls, ResultIsTypeChecked)
{
    EXPECT_TRUE(mentions(proc(kInc, "a <= inc(1);"), "Type mismatch"));
    EXPECT_TRUE(mentions(proc(kInc, "p <= inc(1) = 2 and true; a <= inc(1) = 2;"), "Type mismatch"));
}

TEST(Subprograms_Calls, LiteralsAndAggregatesTakeTheTypeOfTheParameter)
{
    const std::string f = "function count(s : std_logic_vector) return integer is begin return s'length; end; "
                          "function first(c : color) return integer is begin return 1; end; ";
    EXPECT_TRUE(mentions(proc(f, "n <= count((others => '0'));"), "'others' can only be used when the target type has a known length"))
        << "an unconstrained parameter gives an aggregate no length";
    EXPECT_EQ(proc(f, "n <= count(('1', '0', '1'));"), kOk);
    EXPECT_EQ(proc(f, "n <= count(\"0101\"); n <= first(green);"), kOk);
}

TEST(Subprograms_Calls, ParameterlessFunctionsAreUsedAsNames)
{
    const std::string f = "function seven return integer is begin return 7; end; ";
    EXPECT_EQ(proc(f, "n <= seven; n <= seven + 1;"), kOk);
    EXPECT_EQ(arch(f + "constant k : integer := seven; ", ""), kOk) << "a call of a pure function is constant";
}

TEST(Subprograms_Calls, ProceduresAndFunctionsAreNotInterchangeable)
{
    EXPECT_TRUE(mentions(arch("procedure hit is begin null; end;", "process begin n <= hit; wait; end process;"), "is a procedure"));
    EXPECT_TRUE(mentions(arch(kInc, "process begin inc(1); wait; end process;"), "is a function"));
}

TEST(Subprograms_Calls, UnknownAndNonSubprogramNames)
{
    EXPECT_TRUE(mentions(proc("", "nothing(1);"), "'nothing' is not declared"));
    EXPECT_TRUE(mentions(proc("", "n(1);"), "'n' is not a procedure"));
    EXPECT_TRUE(mentions(proc("", "n <= nothing(1);"), "'nothing' is not declared"));
}

TEST(Subprograms_Calls, ANameThatHidesAFunctionIsNotACall)
{
    const std::string message = proc(kInc, "n <= inc(1);", "variable inc : integer;");
    EXPECT_TRUE(mentions(message, "cannot be indexed")) << message;
}

TEST(Subprograms_Calls, CallsInsideConstantsAndIndexes)
{
    EXPECT_EQ(arch(kInc + "constant k : integer := inc(2); signal w : std_logic_vector(inc(2) downto 0); ",
                   "process begin a <= w(inc(0)); wait; end process;"), kOk);
}

TEST(Subprograms_Calls, ProcedureCallsAreConcurrentInAnArchitecture)
{
    const std::string hit = "procedure drive(signal s : out std_logic; v : in std_logic) is begin s <= v; end; ";
    EXPECT_EQ(arch(hit, "drive(a, '1');"), kOk);
    EXPECT_TRUE(mentions(arch(hit, "drive(a);"), "not associated and has no default"));
}

TEST(Subprograms_Calls, ProcedureCallWithoutParentheses)
{
    EXPECT_EQ(arch("procedure tick is begin null; end;", "tick;"), kOk);
}

TEST(Subprograms_Calls, OperatorSymbolNamesAreCalledLikeFunctions)
{
    const std::string f = "function \"+\"(l, r : color) return color is begin return l; end; ";
    EXPECT_EQ(proc(f, "col <= \"+\"(red, green);"), kOk);
    // "and" exists (std_logic_1164 declares it for std_logic and its vectors), but not for an enumeration of the design.
    EXPECT_TRUE(mentions(proc("", "col <= \"and\"(red, green);"), "No overload of '\"and\"' accepts these arguments"));
}

// ===========================================================================
// 4. RETURN
// ===========================================================================

TEST(Subprograms_Return, FunctionsMustReturnAValueOfTheirType)
{
    EXPECT_TRUE(mentions(arch("function f return integer is begin return; end;"), "must return a value"));
    EXPECT_TRUE(mentions(arch("function f return integer is begin return '1'; end;"), "Type mismatch"));
    EXPECT_TRUE(mentions(arch("function f return integer is begin return 300000000000; end;"), "outside the range"));
}

TEST(Subprograms_Return, ProceduresReturnNothing)
{
    EXPECT_EQ(arch("procedure p1 is begin return; end;"), kOk);
    EXPECT_TRUE(mentions(arch("procedure p1 is begin return 1; end;"), "cannot return a value"));
}

TEST(Subprograms_Return, OnlyInsideASubprogram)
{
    EXPECT_TRUE(mentions(proc("", "return;"), "can only be used inside a subprogram"));
}

TEST(Subprograms_Return, EveryPathOfAFunctionMustReturn)
{
    EXPECT_TRUE(mentions(arch("function f return integer is begin null; end;"), "can reach its end without a return"));
    EXPECT_TRUE(mentions(arch("function f(x : integer) return integer is begin if x = 1 then return 1; end if; end;"), "can reach its end"));
    EXPECT_TRUE(mentions(arch("function f(x : integer) return integer is begin if x = 1 then return 1; elsif x = 2 then return 2; end if; end;"),
                         "can reach its end"));
    EXPECT_TRUE(mentions(arch("function f(x : integer) return integer is begin if x = 1 then return 1; else null; end if; end;"), "can reach its end"));
    EXPECT_TRUE(mentions(arch("function f(x : integer) return integer is begin for i in 0 to 3 loop return i; end loop; end;"), "can reach its end"));
    EXPECT_TRUE(mentions(arch("function f(x : integer) return integer is begin while x = 1 loop return 1; end loop; end;"), "can reach its end"));
    EXPECT_TRUE(mentions(arch("function f return integer is begin loop exit; end loop; end;"), "can reach its end"));
    EXPECT_TRUE(mentions(arch("function f return integer is begin l : loop loop exit l; end loop; end loop l; end;"), "can reach its end"));
    EXPECT_TRUE(mentions(arch("function f(c : color) return integer is begin case c is when red => return 1; when others => null; end case; end;"),
                         "can reach its end"));
}

TEST(Subprograms_Return, PathsThatAlwaysReturn)
{
    EXPECT_EQ(arch("function f(x : integer) return integer is begin if x = 1 then return 1; else return 2; end if; end;"), kOk);
    EXPECT_EQ(arch("function f(x : integer) return integer is begin if x = 1 then return 1; elsif x = 2 then return 2; else return 3; end if; end;"), kOk);
    EXPECT_EQ(arch("function f(c : color) return integer is begin case c is when red => return 1; when others => return 2; end case; end;"), kOk);
    EXPECT_EQ(arch("function f return integer is begin loop null; end loop; end;"), kOk) << "a loop without exit never ends";
    EXPECT_EQ(arch("function f return integer is begin loop for i in 0 to 1 loop exit; end loop; end loop; end;"), kOk)
        << "the exit belongs to the inner loop";
    EXPECT_EQ(arch("function f return integer is begin return 1; return 2; end;"), kOk);
    EXPECT_EQ(arch("function f return integer is begin l : loop while true loop exit; end loop; return 1; end loop l; end;"), kOk);
}

// ===========================================================================
// 5. RECURSION
// ===========================================================================

TEST(Subprograms_Recursion, ASubprogramCanCallItself)
{
    const std::string fact = "function fact(x : integer) return integer is begin if x <= 1 then return 1; else return x * fact(x - 1); end if; end; ";
    EXPECT_EQ(proc(fact, "n <= fact(5);"), kOk);
}

TEST(Subprograms_Recursion, MutualRecursionNeedsAForwardDeclaration)
{
    const std::string pair = "function odd(x : integer) return boolean; "
                             "function even(x : integer) return boolean is begin if x = 0 then return true; else return odd(x - 1); end if; end; "
                             "function odd(x : integer) return boolean is begin if x = 0 then return false; else return even(x - 1); end if; end; ";
    EXPECT_EQ(proc(pair, "p <= even(4);"), kOk);

    const std::string withoutDeclaration = "function even(x : integer) return boolean is begin return odd(x); end; "
                                           "function odd(x : integer) return boolean is begin return even(x); end; ";
    EXPECT_TRUE(mentions(arch(withoutDeclaration), "'odd' is not declared"));
}

// ===========================================================================
// 6. OVERLOADING
// ===========================================================================

TEST(Subprograms_Overloading, ParameterTypesChooseTheOverload)
{
    const std::string f = "function show(x : integer) return integer is begin return 1; end; "
                          "function show(x : color) return integer is begin return 2; end; "
                          "function show(x : std_logic) return integer is begin return 3; end; ";
    EXPECT_EQ(proc(f, "n <= show(1); n <= show(red); n <= show('1'); n <= show(a); n <= show(col);"), kOk);
}

TEST(Subprograms_Overloading, TheNumberOfParametersChoosesTheOverload)
{
    const std::string f = "function pick(x : integer) return integer is begin return x; end; "
                          "function pick(x, y : integer) return integer is begin return y; end; ";
    EXPECT_EQ(proc(f, "n <= pick(1); n <= pick(1, 2); n <= pick(y => 2, x => 1);"), kOk);
}

TEST(Subprograms_Overloading, TheExpectedResultTypeChoosesTheOverload)
{
    const std::string f = "function get return integer is begin return 1; end; "
                          "function get return boolean is begin return true; end; ";
    EXPECT_EQ(proc(f, "n <= get; p <= get;"), kOk);
    EXPECT_EQ(proc(f, "if get then null; end if;"), kOk);
}

TEST(Subprograms_Overloading, ProceduresAndFunctionsCanShareAName)
{
    const std::string f = "function twice(x : integer) return integer is begin return x * 2; end; "
                          "procedure twice(x : integer; y : out integer) is begin y := x * 2; end; ";
    EXPECT_EQ(proc(f, "n <= twice(2); twice(2, n2);", "variable n2 : integer;"), kOk);
}

TEST(Subprograms_Overloading, NoOverloadFitsListsTheCandidates)
{
    const std::string f = "function show(x : integer) return integer is begin return 1; end; "
                          "function show(x : color) return integer is begin return 2; end; ";
    const std::string message = proc(f, "n <= show(v4);");
    EXPECT_TRUE(mentions(message, "No overload of 'show' accepts these arguments")) << message;
    EXPECT_TRUE(mentions(message, "show(integer) return integer")) << message;
    EXPECT_TRUE(mentions(message, "show(color) return integer")) << message;
}

TEST(Subprograms_Overloading, AmbiguousCallsAreRejected)
{
    const std::string f = "type t1 is (x1, y1, both); type t2 is (x2, y2, both); "
                          "function k(v : t1) return integer is begin return 1; end; "
                          "function k(v : t2) return integer is begin return 2; end; ";
    const std::string message = arch(f, "process begin n <= k(both); wait; end process;");
    EXPECT_TRUE(mentions(message, "is ambiguous")) << message;
    EXPECT_EQ(arch(f, "process begin n <= k(x1); n <= k(y2); wait; end process;"), kOk);
}

TEST(Subprograms_Overloading, AnInnerDeclarationHidesAnOuterOneWithTheSameProfile)
{
    const std::string outer = "function f return integer is begin return 1; end; ";
    const std::string inner = "function f return integer is begin return 2; end; ";
    EXPECT_EQ(arch(outer, "process " + inner + " begin n <= f; wait; end process;"), kOk);
}

TEST(Subprograms_Overloading, InnerOverloadsAddToOuterOnesWithOtherProfiles)
{
    const std::string outer = "function f(x : integer) return integer is begin return 1; end; ";
    const std::string inner = "function f(x : color) return integer is begin return 2; end; ";
    EXPECT_EQ(arch(outer, "process " + inner + " begin n <= f(1); n <= f(red); wait; end process;"), kOk);
}

// ===========================================================================
// 7. OPERATOR OVERLOADING
// ===========================================================================

TEST(Subprograms_Operators, UserOperatorsOnUserTypes)
{
    const std::string ops = "function \"+\"(l, r : color) return color is begin return l; end; "
                            "function \"-\"(l : color) return color is begin return l; end; "
                            "function \"=\"(l, r : color) return boolean is begin return true; end; "
                            "function \"and\"(l, r : color) return color is begin return l; end; "
                            "function \"not\"(l : color) return color is begin return l; end; "
                            "function \"abs\"(l : color) return color is begin return l; end; "
                            "function \"<\"(l, r : color) return boolean is begin return true; end; "
                            "function \"&\"(l, r : color) return color is begin return l; end; "
                            "function \"**\"(l : color; r : integer) return color is begin return l; end; "
                            "function \"sll\"(l : color; r : integer) return color is begin return l; end; ";
    EXPECT_EQ(proc(ops, "col <= red + green; col <= -red; p <= red = green; col <= red and green; col <= not red; col <= abs red; "
                        "p <= red < green; col <= red & green; col <= red ** 2; col <= red sll 1;"), kOk);
}

TEST(Subprograms_Operators, PredefinedOperatorsStayVisible)
{
    const std::string ops = "function \"+\"(l, r : color) return color is begin return l; end; ";
    EXPECT_EQ(proc(ops, "n <= m + 1; u4 <= u4 + 1; col <= red + blue;"), kOk);
}

TEST(Subprograms_Operators, AUserOperatorOnPredefinedTypesTakesPrecedence)
{
    const std::string ops = "function \"+\"(l, r : integer) return boolean is begin return true; end; ";
    EXPECT_EQ(proc(ops, "p <= n + m;"), kOk);
    EXPECT_TRUE(mentions(proc(ops, "n <= n + m;"), "Type mismatch"));
}

TEST(Subprograms_Operators, OperandsThatNeedAContextTakeTheTypeOfTheFunction)
{
    const std::string ops = "function \"&\"(l : std_logic; r : color) return color is begin return r; end; ";
    EXPECT_EQ(proc(ops, "col <= '1' & red;"), kOk);
}

TEST(Subprograms_Operators, UserReductionsAndConditionOperators)
{
    const std::string ops = "function \"and\"(l : color) return boolean is begin return true; end; "
                            "function \"??\"(l : color) return boolean is begin return true; end; ";
    EXPECT_EQ(proc(ops, "p <= and red; p <= ?? green;"), kOk);
}

TEST(Subprograms_Operators, UserOperatorsAreNotFoldedIntoConstants)
{
    const std::string ops = "function \"+\"(l, r : integer) return integer is begin return 0; end; ";
    EXPECT_EQ(arch(ops + "constant k : integer := 1 + 2; signal w : std_logic_vector(k downto 0); "), kOk)
        << "the value of k is unknown, so w has bounds that depend on it";
}

TEST(Subprograms_Operators, OperatorProfilesAreChecked)
{
    EXPECT_TRUE(mentions(arch("function \"not\"(l, r : color) return color is begin return l; end;"), "takes 1 parameter"));
    EXPECT_TRUE(mentions(arch("function \"=\"(l : color) return boolean is begin return true; end;"), "takes 2 parameter"));
    EXPECT_TRUE(mentions(arch("function \"and\"(l, r, s : color) return color is begin return l; end;"), "takes 1 or 2 parameter"));
    EXPECT_TRUE(mentions(arch("function \"foo\"(l, r : color) return color is begin return l; end;"), "not an operator that can be overloaded"));
    EXPECT_TRUE(mentions(arch("procedure \"+\"(l, r : color) is begin null; end;"), "Only a function can overload"));
}

// ===========================================================================
// 8. PURITY AND BODY RULES
// ===========================================================================

TEST(Subprograms_Purity, APureFunctionCannotReadWhatItDoesNotDeclare)
{
    EXPECT_TRUE(mentions(arch("function f return integer is begin return n; end;"), "cannot access 'n'"));
    EXPECT_TRUE(mentions(arch("function f return std_logic is begin return a; end;"), "cannot access 'a'"));
    EXPECT_TRUE(mentions(proc("", "n <= f;", "variable v : integer; function f return integer is begin return v; end;"), "cannot access 'v'"));
}

TEST(Subprograms_Purity, APureFunctionMayUseItsOwnConstantsVariablesAndParameters)
{
    EXPECT_EQ(arch("constant k : integer := 3; function f(x : integer) return integer is variable t : integer := 1; begin t := t + x + k; return t; end;"), kOk);
}

TEST(Subprograms_Purity, AnImpureFunctionMayReadSignalsAndVariables)
{
    EXPECT_EQ(arch("impure function f return integer is begin return n; end; ", "process begin m <= f; wait; end process;"), kOk);
    EXPECT_EQ(proc("", "n <= f;", "variable v : integer := 4; impure function f return integer is begin v := v + 1; return v; end;"), kOk);
}

TEST(Subprograms_Purity, APureFunctionCannotAssignOutsideVariables)
{
    EXPECT_TRUE(mentions(proc("", "n <= f;", "variable v : integer; function f return integer is begin v := 1; return 1; end;"), "cannot access 'v'"));
}

TEST(Subprograms_Purity, APureFunctionCannotCallAnImpureOne)
{
    const std::string message = arch("impure function g return integer is begin return n; end; "
                                     "function f return integer is begin return g; end;");
    EXPECT_TRUE(mentions(message, "cannot call the impure function 'g'")) << message;
    EXPECT_EQ(arch("impure function g return integer is begin return n; end; "
                   "impure function f return integer is begin return g; end;"), kOk);
}

TEST(Subprograms_Purity, FunctionsCannotAssignSignalsOrWait)
{
    EXPECT_TRUE(mentions(arch("impure function f return integer is begin n <= 1; return 1; end;"), "cannot assign signals"));
    EXPECT_TRUE(mentions(arch("impure function f return integer is begin wait for 1 ns; return 1; end;"), "cannot contain a wait"));
}

TEST(Subprograms_Purity, AFunctionCannotCallAProcedureThatWaits)
{
    const std::string waits = "procedure pause is begin wait for 1 ns; end; ";
    const std::string message = arch(waits + "impure function f return integer is begin pause; return 1; end;");
    EXPECT_TRUE(mentions(message, "cannot call the procedure 'pause', which contains a wait")) << message;
    EXPECT_TRUE(mentions(arch(waits + "procedure outer is begin pause; end; impure function f return integer is begin outer; return 1; end;"),
                         "cannot call the procedure 'outer', which contains a wait"))
        << "the wait is found through another procedure";
}

TEST(Subprograms_Purity, ProceduresThatWaitNeedAProcessWithoutASensitivityList)
{
    const std::string waits = "procedure pause is begin wait for 1 ns; end; ";
    EXPECT_EQ(arch(waits, "process begin pause; end process;"), kOk);
    EXPECT_TRUE(mentions(arch(waits, "process (a) begin pause; end process;"), "has a sensitivity list"));
    EXPECT_TRUE(mentions(arch(waits, "process (all) begin pause; end process;"), "has a sensitivity list"));
    EXPECT_EQ(arch("procedure tick is begin null; end; ", "process (a) begin tick; end process; tick;"), kOk);
}

TEST(Subprograms_Purity, AConcurrentCallMayCallAProcedureThatWaits)
{
    // LRM 11.4: the equivalent process of a concurrent procedure call has no sensitivity list, only a final wait statement.
    const std::string waits = "procedure pause is begin wait for 1 ns; end; ";
    EXPECT_EQ(arch(waits, "pause;"), kOk);

    // The clock generator of many testbenches.
    const std::string clockGenerator =
        "procedure clk_gen(signal clk : out std_logic; constant half : time) is begin "
        "loop clk <= '0'; wait for half; clk <= '1'; wait for half; end loop; end procedure; ";
    EXPECT_EQ(arch(clockGenerator, "clk_gen(a, 5 ns);"), kOk);
    EXPECT_EQ(arch(clockGenerator, "gen : clk_gen(clk => a, half => 5 ns);"), kOk);
}

TEST(Subprograms_Purity, WaitInsideAProcedureIsFine)
{
    EXPECT_EQ(arch("procedure pause is begin wait until a = '1'; wait on b; end;"), kOk);
}

// ===========================================================================
// 9. SIGNAL AND VARIABLE PARAMETERS
// ===========================================================================

TEST(Subprograms_SignalParameters, TheCallDrivesItsOutputSignals)
{
    const std::string drive = "procedure drive(signal s : out integer; v : in integer) is begin s <= v; end; ";
    EXPECT_EQ(arch(drive, "process begin drive(n, 1); wait; end process;"), kOk);

    const std::string message = arch(drive, "process begin drive(n, 1); wait; end process; process begin drive(n, 2); wait; end process;");
    EXPECT_TRUE(mentions(message, "cannot have several drivers")) << message;
}

TEST(Subprograms_SignalParameters, ResolvedSignalsMayHaveSeveralCallers)
{
    const std::string drive = "procedure drive(signal s : out std_logic; v : in std_logic) is begin s <= v; end; ";
    EXPECT_EQ(arch(drive, "process begin drive(a, '1'); wait; end process; process begin drive(a, '0'); wait; end process;"), kOk);
}

TEST(Subprograms_SignalParameters, ASignalParameterNeedsASignalActual)
{
    const std::string drive = "procedure drive(signal s : out integer) is begin s <= 1; end; ";
    EXPECT_TRUE(mentions(arch(drive, "process variable v : integer; begin drive(v); wait; end process;"), "must be a signal"));
    EXPECT_TRUE(mentions(arch(drive + "constant k : integer := 1; ", "process begin drive(k); wait; end process;"), "must be a signal"));
    EXPECT_TRUE(mentions(arch(drive, "process begin drive(1); wait; end process;"), "must be a signal"));
}

TEST(Subprograms_SignalParameters, AnOutputNeedsAWritableActual)
{
    const std::string design =
        "entity t is port (i : in std_logic; o : out std_logic); end t; "
        "architecture r of t is procedure drive(signal s : out std_logic) is begin s <= '1'; end; begin ";

    EXPECT_EQ(analysisError(design + "drive(o); end r;"), kOk);
    const std::string message = analysisError(design + "drive(i); end r;");
    EXPECT_TRUE(mentions(message, "Port 'i' is an input and cannot be assigned")) << message;
}

TEST(Subprograms_VariableParameters, ActualsMustBeVariables)
{
    const std::string bump = "procedure bump(variable v : inout integer) is begin v := v + 1; end; ";
    EXPECT_EQ(arch(bump, "process variable k : integer := 0; begin bump(k); wait; end process;"), kOk);
    EXPECT_TRUE(mentions(arch(bump, "process begin bump(n); wait; end process;"), "must be a variable"));
    EXPECT_TRUE(mentions(arch(bump, "process begin bump(3); wait; end process;"), "must be a variable"));
}

TEST(Subprograms_VariableParameters, OutputParametersReceiveTheirValue)
{
    const std::string get = "procedure get(x : out integer) is begin x := 7; end; ";
    EXPECT_EQ(arch(get, "process variable k : integer; begin get(k); wait; end process;"), kOk);
    EXPECT_TRUE(mentions(arch(get, "process begin get(n); wait; end process;"), "must be a variable"));
    const std::string typed = arch(get, "process variable r : std_logic; begin get(r); wait; end process;");
    EXPECT_TRUE(mentions(typed, "Type mismatch")) << typed;
}

TEST(Subprograms_VariableParameters, AnInputActualCannotReceiveAnOutput)
{
    const std::string get = "procedure get(x : out integer) is begin x := 7; end; ";
    const std::string message = arch(get, "process procedure inner(k : in integer) is begin get(k); end; begin wait; end process;");
    EXPECT_TRUE(mentions(message, "must be a variable")) << message;
}

// ===========================================================================
// 10. WHERE A PROCEDURE MAY ASSIGN SIGNALS
// ===========================================================================

TEST(Subprograms_Assignments, AProcedureOutsideAProcessOnlyAssignsItsOwnSignalParameters)
{
    const std::string message = arch("procedure set is begin n <= 1; end;");
    EXPECT_TRUE(mentions(message, "is not declared in a process")) << message;
    EXPECT_EQ(arch("procedure set(signal s : out integer) is begin s <= 1; end;"), kOk);
}

TEST(Subprograms_Assignments, AProcedureInAProcessMayAssignTheSignalsOfThatProcess)
{
    EXPECT_EQ(arch("", "process procedure set is begin n <= 1; end; begin set; wait; end process;"), kOk);
}

TEST(Subprograms_Assignments, TheProcessOfTheProcedureIsTheDriver)
{
    const std::string message = arch("", "process procedure set is begin n <= 1; end; begin set; wait; end process; process begin n <= 2; wait; end process;");
    EXPECT_TRUE(mentions(message, "cannot have several drivers")) << message;
}

TEST(Subprograms_Assignments, ProceduresNestedInAProcedureInAProcess)
{
    EXPECT_EQ(arch("", "process procedure outer is procedure inner is begin n <= 1; end; begin inner; end; begin outer; wait; end process;"), kOk);
}

TEST(Subprograms_Assignments, LoopsInsideBodiesAreLocal)
{
    EXPECT_TRUE(mentions(proc("procedure p1 is begin exit; end;", "null;"), "can only be used inside a loop"));
    EXPECT_TRUE(mentions(proc("procedure p1 is begin next; end;", "null;", ""), "can only be used inside a loop"));
    EXPECT_TRUE(mentions(arch("procedure p1 is begin exit; end;", "process begin for i in 0 to 1 loop null; end loop; wait; end process;"),
                         "can only be used inside a loop"))
        << "an exit in a procedure never leaves the loop of its caller";
}

// ===========================================================================
// 11. FUNCTION CALL RESULTS IN EVERY CONTEXT
// ===========================================================================

TEST(Subprograms_Contexts, ResultsFeedOperatorsIndexesAndConditions)
{
    const std::string f = "function five return integer is begin return 5; end; "
                          "function flag(x : integer) return boolean is begin return x = 5; end; "
                          "function bit_of(x : integer) return std_logic is begin return '1'; end; ";
    EXPECT_EQ(proc(f, "n <= five + 1; if flag(five) then a <= bit_of(0); end if; v4(five - 4) <= bit_of(1); assert flag(1) ;"), kOk);
    EXPECT_EQ(proc(f, "while flag(n) loop exit when flag(1); end loop; case five is when 5 => null; when others => null; end case;"), kOk);
    EXPECT_EQ(proc(f, "for i in 0 to five loop null; end loop;"), kOk);
}

TEST(Subprograms_Contexts, UnconstrainedResultsAdaptToTheTarget)
{
    const std::string f = "function copy(v : std_logic_vector) return std_logic_vector is begin return v; end; ";
    EXPECT_EQ(proc(f, "v4 <= copy(v4);"), kOk);
}

TEST(Subprograms_Contexts, PrivateStateOfAFunctionBodyIsIsolated)
{
    const std::string f = "function f(x : integer) return integer is variable t : integer := 0; begin t := x; return t; end; ";
    EXPECT_EQ(proc(f, "n <= f(1); m <= f(2);"), kOk);
}
