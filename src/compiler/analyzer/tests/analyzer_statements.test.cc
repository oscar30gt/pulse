// analyzer_statements.test.cc — processes, sequential statements, case/with choices, loops,
// component instantiation and the multiple-driver rule.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;
using TestUtil::inArchitecture;

namespace
{
    constexpr const char* kOk = "<no error>";

    const std::string kDecls =
        "signal a, b, c : std_logic; signal n, m : integer; signal p : boolean; signal t : time; "
        "signal v4 : std_logic_vector(3 downto 0); signal u4 : unsigned(3 downto 0); "
        "type color is (red, green, blue); signal col : color; ";

    /// Analyzes `statements` inside one process (with optional process declarations).
    std::string inProc(const std::string& statements, const std::string& processDecls = "", const std::string& extraDecls = "")
    {
        return analysisError(inArchitecture(kDecls + extraDecls, "process " + processDecls + " begin " + statements + " wait; end process;"));
    }

    std::string inArch(const std::string& body, const std::string& extraDecls = "")
    {
        return analysisError(inArchitecture(kDecls + extraDecls, body));
    }

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }
}

// ---- Processes ------------------------------------------------------------------------------------

TEST(Semantic_Process, SensitivityListsNameSignals)
{
    EXPECT_EQ(inArch("process (a, b) begin c <= a and b; end process;"), kOk);
    EXPECT_EQ(inArch("process (all) begin c <= a; end process;"), kOk);
    EXPECT_TRUE(mentions(inArch("process (missing) begin c <= a; end process;"), "not declared"));
    EXPECT_NE(inArch("process (n + 1) begin c <= a; end process;"), kOk);
    EXPECT_TRUE(mentions(inArch("process (k) begin c <= a; end process;", "constant k : integer := 1;"), "sensitivity"));
}

TEST(Semantic_Process, WaitIsIncompatibleWithASensitivityList)
{
    EXPECT_TRUE(mentions(inArch("process (a) begin wait for 10 ns; end process;"), "cannot contain wait"));
    EXPECT_TRUE(mentions(inArch("process (all) begin if a = '1' then wait; end if; end process;"), "cannot contain wait"));
    EXPECT_EQ(inArch("process begin wait on a; c <= a; end process;"), kOk);
}

TEST(Semantic_Process, LocalDeclarationsAreScopedToTheProcess)
{
    EXPECT_EQ(inProc("k := 3;", "variable k : integer;"), kOk);
    EXPECT_TRUE(mentions(inArch("process variable k : integer; begin wait; end process; process begin k := 1; wait; end process;"), "not declared"));
}

TEST(Semantic_Process, LocalTypesShadowArchitectureTypes)
{
    EXPECT_EQ(inArch("process type color is (x, y); variable v : color; begin v := x; wait; end process;"), kOk);
}

TEST(Semantic_Process, LabelsAreUniqueAndAreNotValues)
{
    EXPECT_TRUE(mentions(inArch("p1 : process begin wait; end process; p1 : process begin wait; end process;"), "already declared"));
    EXPECT_TRUE(mentions(inArch("p1 : process begin n <= p1; wait; end process;"), "label"));
    EXPECT_TRUE(mentions(inArch("a : process begin wait; end process;"), "already declared"));
}

// ---- Assignments ----------------------------------------------------------------------------------

TEST(Semantic_Assignments, VariablesUseColonEqualsAndSignalsUseArrow)
{
    EXPECT_EQ(inProc("k := k + 1; c <= a;", "variable k : integer := 0;"), kOk);
    EXPECT_TRUE(mentions(inProc("k <= 1;", "variable k : integer;"), "variable; assign it with ':='"));
    EXPECT_TRUE(mentions(inProc("n := 1;"), "signal; assign it with '<='"));
    EXPECT_TRUE(mentions(inProc("k := 1;", "constant k : integer := 0;"), "constant"));
}

TEST(Semantic_Assignments, VariablesAreTypeChecked)
{
    EXPECT_TRUE(mentions(inProc("k := '1';", "variable k : integer;"), "Type mismatch"));
    EXPECT_TRUE(mentions(inProc("k := 256;", "variable k : integer range 0 to 255;"), "outside the range"));
    EXPECT_EQ(inProc("k(3) := '1'; k(1 downto 0) := \"01\";", "variable k : std_logic_vector(3 downto 0);"), kOk);
}

TEST(Semantic_Assignments, VariableInitialValuesAreConstant)
{
    EXPECT_TRUE(mentions(inProc("wait;", "variable k : integer := n;"), "must be constant"));
}

// ---- if / case ------------------------------------------------------------------------------------

TEST(Semantic_If, ConditionsAreBooleanOrStdLogic)
{
    EXPECT_EQ(inProc("if a = '1' then c <= b; elsif p then c <= a; elsif b then c <= '0'; else c <= '1'; end if;"), kOk);
    EXPECT_TRUE(mentions(inProc("if n then c <= a; end if;"), "must be boolean"));
    EXPECT_TRUE(mentions(inProc("if a = '1' then c <= 1; end if;"), "Type mismatch"));
}

TEST(Semantic_Case, EnumerationCoverage)
{
    EXPECT_EQ(inProc("case col is when red => a <= '1'; when green | blue => a <= '0'; end case;"), kOk);
    EXPECT_EQ(inProc("case col is when red => a <= '1'; when others => a <= '0'; end case;"), kOk);

    const std::string missing = inProc("case col is when red => a <= '1'; when green => a <= '0'; end case;");
    EXPECT_TRUE(mentions(missing, "blue is missing")) << missing;
}

TEST(Semantic_Case, IntegerChoices)
{
    EXPECT_EQ(inProc("case n is when 0 => a <= '1'; when 1 to 5 => a <= '0'; when others => null; end case;"), kOk);
    EXPECT_TRUE(mentions(inProc("case n is when 0 => a <= '1'; when 1 to 5 => a <= '0'; end case;"), "do not cover"));
    EXPECT_TRUE(mentions(inProc("case n is when 1 | 1 => a <= '1'; when others => null; end case;"), "more than once"));
    EXPECT_TRUE(mentions(inProc("case n is when 1 to 4 => a <= '1'; when 3 => a <= '0'; when others => null; end case;"), "more than once"));
}

TEST(Semantic_Case, SubtypeSelectorsCoverOnlyTheirRange)
{
    EXPECT_EQ(inProc("case k is when 0 to 3 => a <= '1'; end case;", "variable k : integer range 0 to 3;"), kOk);
    EXPECT_TRUE(mentions(inProc("case k is when 0 to 4 => a <= '1'; end case;", "variable k : integer range 0 to 3;"), "outside"));
}

TEST(Semantic_Case, ChoicesMustBeConstantAndOfTheSelectorType)
{
    EXPECT_TRUE(mentions(inProc("case n is when m => a <= '1'; when others => null; end case;"), "constant"));
    EXPECT_TRUE(mentions(inProc("case n is when red => a <= '1'; when others => null; end case;"), "Type mismatch"));
    EXPECT_TRUE(mentions(inProc("case col is when 1 => a <= '1'; when others => null; end case;"), "Type mismatch"));
}

TEST(Semantic_Case, ConstantsAreValidChoices)
{
    EXPECT_EQ(inProc("case n is when k => a <= '1'; when others => null; end case;", "", "constant k : integer := 3;"), kOk);
}

TEST(Semantic_Case, OthersMustBeLast)
{
    EXPECT_TRUE(mentions(inProc("case col is when others => null; when red => null; end case;"), "must be the last"));
}

TEST(Semantic_Case, StdLogicSelectorCoversNineValues)
{
    EXPECT_EQ(inProc("case a is when '0' => c <= '0'; when '1' => c <= '1'; when others => c <= 'X'; end case;"), kOk);
    EXPECT_TRUE(mentions(inProc("case a is when '0' => c <= '0'; when '1' => c <= '1'; end case;"), "is missing"));
}

TEST(Semantic_Case, VectorSelectorsNeedOthers)
{
    EXPECT_EQ(inProc("case v4 is when \"0000\" => a <= '1'; when \"0001\" => a <= '0'; when others => null; end case;"), kOk);
    EXPECT_TRUE(mentions(inProc("case v4 is when \"0000\" => a <= '1'; end case;"), "others"));
    EXPECT_TRUE(mentions(inProc("case v4 is when \"0000\" => a <= '1'; when \"0000\" => a <= '0'; when others => null; end case;"), "more than once"));
    EXPECT_TRUE(mentions(inProc("case v4 is when \"000\" => a <= '1'; when others => null; end case;"), "3 element"));
}

TEST(Semantic_Case, SelectorTypeMustBeDiscrete)
{
    EXPECT_TRUE(mentions(inProc("case t is when others => null; end case;"), "selector"));
}

TEST(Semantic_Case, BodiesAreAnalyzed)
{
    EXPECT_TRUE(mentions(inProc("case col is when others => a <= n; end case;"), "Type mismatch"));
}

// ---- Loops ----------------------------------------------------------------------------------------

TEST(Semantic_Loops, ForLoopParameterIsAnInteger)
{
    EXPECT_EQ(inProc("for i in 0 to 3 loop v4(i) <= '1'; n <= i; end loop;"), kOk);
    EXPECT_EQ(inProc("for i in v4'range loop v4(i) <= '0'; end loop;"), kOk);
    EXPECT_EQ(inProc("for i in 3 downto 0 loop v4(i) <= '0'; end loop;"), kOk);
    EXPECT_EQ(inProc("for x in color loop col <= x; end loop;"), kOk);
}

TEST(Semantic_Loops, LoopParameterIsReadOnlyAndScoped)
{
    EXPECT_TRUE(mentions(inProc("for i in 0 to 3 loop i := 1; end loop;"), "loop parameter"));
    EXPECT_TRUE(mentions(inProc("for i in 0 to 3 loop null; end loop; n <= i;"), "not declared"));
}

TEST(Semantic_Loops, LoopParameterIndexIsBoundsChecked)
{
    EXPECT_EQ(inProc("for i in 0 to 3 loop v4(i) <= '1'; end loop;"), kOk);
}

TEST(Semantic_Loops, ForRangeMustBeDiscrete)
{
    EXPECT_TRUE(mentions(inProc("for i in 0.0 to 1.0 loop null; end loop;"), "integer or enumeration"));
    EXPECT_TRUE(mentions(inProc("for i in real loop null; end loop;"), "cannot be used as a range"));
}

TEST(Semantic_Loops, WhileAndPlainLoopsWithExitAndNext)
{
    EXPECT_EQ(inProc("while n < 10 loop n <= n + 1; end loop;"), kOk);
    EXPECT_EQ(inProc("loop exit when a = '1'; end loop;"), kOk);
    EXPECT_EQ(inProc("outer : for i in 0 to 3 loop for j in 0 to 3 loop next outer when i = j; exit outer; end loop; end loop outer;"), kOk);
    EXPECT_TRUE(mentions(inProc("while n loop null; end loop;"), "must be boolean"));
}

TEST(Semantic_Loops, ExitAndNextNeedALoop)
{
    EXPECT_TRUE(mentions(inProc("exit;"), "inside a loop"));
    EXPECT_TRUE(mentions(inProc("next;"), "inside a loop"));
    EXPECT_TRUE(mentions(inProc("loop exit nowhere; end loop;"), "no enclosing loop labeled 'nowhere'"));
    EXPECT_TRUE(mentions(inProc("loop exit when n; end loop;"), "must be boolean"));
}

TEST(Semantic_Loops, LoopLabelsBelongToTheRegion)
{
    EXPECT_TRUE(mentions(inProc("l : while n < 1 loop exit; end loop; l : loop exit; end loop;"), "already declared"));
}

TEST(Semantic_Loops, LabelsOfEnclosingLoopsCannotRepeat)
{
    EXPECT_TRUE(mentions(inProc("l : loop l : loop exit; end loop; end loop;"), "already"));
}

// ---- Wait, assert, report -------------------------------------------------------------------------

TEST(Semantic_Wait, ArgumentsAreTypeChecked)
{
    EXPECT_EQ(analysisError(inArchitecture(kDecls, "process begin wait for 10 ns; wait on a, b; wait until a = '1'; wait on a until n = 1 for 1 us; end process;")), kOk);
    EXPECT_TRUE(mentions(analysisError(inArchitecture(kDecls, "process begin wait for 10; end process;")), "time"));
    EXPECT_TRUE(mentions(analysisError(inArchitecture(kDecls + "constant k : integer := 1;", "process begin wait on k; end process;")), "signals and ports"));
    EXPECT_TRUE(mentions(analysisError(inArchitecture(kDecls, "process begin wait until n; end process;")), "must be boolean"));
    EXPECT_TRUE(mentions(analysisError(inArchitecture(kDecls, "process begin wait for -5 ns; end process;")), "negative"));
}

TEST(Semantic_Assert, ConditionAndSeverity)
{
    const std::string strings = TestUtil::stringTypes();
    EXPECT_EQ(inProc("assert a = '1' report \"a low\" severity error; assert p; report \"hi\"; report \"x\" severity note;", "", strings), kOk);
    EXPECT_TRUE(mentions(inProc("assert n;", "", strings), "must be boolean"));
    EXPECT_TRUE(mentions(inProc("assert p severity n;", "", strings), "severity_level"));
    EXPECT_TRUE(mentions(inProc("report \"x\" severity red;", "", strings), "severity"));
}

TEST(Semantic_Assert, MessagesAreStringsAndNeedTheStringTypeToBeDeclared)
{
    const std::string message = inProc("report \"hi\";");
    EXPECT_TRUE(mentions(message, "the type 'string' is not declared")) << message;
    EXPECT_TRUE(mentions(inProc("assert p report \"hi\";"), "the type 'string' is not declared"));
    EXPECT_EQ(inProc("assert p;"), kOk) << "an assertion without a message needs no string type";
}

// ---- Selected assignment --------------------------------------------------------------------------

TEST(Semantic_With, ChoicesAreCheckedLikeCase)
{
    EXPECT_EQ(inArch("with col select a <= '1' when red, '0' when green | blue;"), kOk);
    EXPECT_EQ(inArch("with n select a <= '1' when 0, '0' when others;"), kOk);
    EXPECT_TRUE(mentions(inArch("with col select a <= '1' when red;"), "do not cover"));
    EXPECT_TRUE(mentions(inArch("with n select a <= n when 0, b when others;"), "Type mismatch"));
    EXPECT_TRUE(mentions(inArch("with n select k <= '1' when others;", "constant k : integer := 0;"), "constant"));
}

TEST(Semantic_With, VectorSelectorAndTarget)
{
    EXPECT_EQ(inArch("with v4 select a <= '1' when \"0000\", '0' when others;"), kOk);
    EXPECT_EQ(inArch("with n select v4 <= \"0001\" when 0, (others => '0') when others;"), kOk);
    EXPECT_TRUE(mentions(inArch("with n select v4 <= \"001\" when 0, (others => '0') when others;"), "3 element"));
}

// ---- Component instantiation ----------------------------------------------------------------------

namespace
{
    const std::string kAdder =
        "entity adder is port (x, y : in std_logic_vector(3 downto 0); s : out std_logic_vector(3 downto 0); cout : out std_logic); end adder;\n";

    std::string design(const std::string& portMap, const std::string& extra = "")
    {
        return analysisError(kAdder + "entity top is end top; architecture r of top is "
            "component adder port (x, y : in std_logic_vector(3 downto 0); s : out std_logic_vector(3 downto 0); cout : out std_logic); end component; "
            "signal p, q, r : std_logic_vector(3 downto 0); signal co : std_logic; signal n : integer; " + extra +
            " begin u1 : adder port map (" + portMap + "); end r;");
    }
}

TEST(Semantic_Instances, ValidPortMaps)
{
    EXPECT_EQ(design("x => p, y => q, s => r, cout => co"), kOk);
    EXPECT_EQ(design("x => p, y => not q, s => r, cout => open"), kOk);
    EXPECT_EQ(design("x => \"0001\", y => p(3 downto 0), s => r(3 downto 0), cout => co"), kOk);
}

TEST(Semantic_Instances, UnknownComponentPortAndDuplicates)
{
    EXPECT_TRUE(mentions(analysisError("entity top is end top; architecture r of top is begin u : ghost port map (a => b); end r;"), "Unknown component 'ghost'"));
    EXPECT_TRUE(mentions(design("x => p, y => q, s => r, cout => co, z => p"), "no port named 'z'"));
    EXPECT_TRUE(mentions(design("x => p, x => q, y => q, s => r, cout => co"), "connected twice"));
}

TEST(Semantic_Instances, InputsMustBeConnected)
{
    const std::string message = design("x => p, s => r, cout => co");
    EXPECT_TRUE(mentions(message, "Input port 'y'")) << message;
    EXPECT_TRUE(mentions(message, "not connected")) << message;
    EXPECT_TRUE(mentions(design("x => p, y => open, s => r, cout => co"), "cannot be left open"));
}

TEST(Semantic_Instances, ActualsAreTypeChecked)
{
    EXPECT_TRUE(mentions(design("x => co, y => q, s => r, cout => co"), "Type mismatch"));
    EXPECT_TRUE(mentions(design("x => p(2 downto 0), y => q, s => r, cout => co"), "Length mismatch"));
    EXPECT_TRUE(mentions(design("x => p, y => q, s => r, cout => p"), "Type mismatch"));
    EXPECT_TRUE(mentions(design("x => ghost, y => q, s => r, cout => co"), "'ghost' is not declared"));
}

TEST(Semantic_Instances, OutputsNeedWritableNames)
{
    EXPECT_TRUE(mentions(design("x => p, y => q, s => p and q, cout => co"), "must be a signal"));
    EXPECT_TRUE(mentions(design("x => p, y => q, s => r, cout => k", "constant k : std_logic := '0';"), "constant"));
}

TEST(Semantic_Instances, InstanceLabelsShareTheNamespace)
{
    EXPECT_TRUE(mentions(design("x => p, y => q, s => r, cout => co", "signal u1 : integer;"), "already declared"));
}

// ---- Multiple drivers -----------------------------------------------------------------------------

TEST(Semantic_Drivers, ResolvedTypesMayHaveSeveralDrivers)
{
    EXPECT_EQ(inArch("a <= '1'; a <= '0';"), kOk);
    EXPECT_EQ(inArch("v4 <= \"0000\"; v4 <= \"1111\";"), kOk);
    EXPECT_EQ(inArch("process begin a <= '1'; wait; end process; process begin a <= '0'; wait; end process;"), kOk);
}

TEST(Semantic_Drivers, UnresolvedTypesAllowOneSourceOnly)
{
    const std::string message = inArch("n <= 1; n <= 2;");
    EXPECT_TRUE(mentions(message, "Signal 'n'")) << message;
    EXPECT_TRUE(mentions(message, "several drivers")) << message;
    EXPECT_TRUE(mentions(message, "2 sources")) << message;

    EXPECT_TRUE(mentions(inArch("p1 : process begin n <= 1; wait; end process; p2 : process begin n <= 2; wait; end process;"), "process 'p1', process 'p2'"));
    EXPECT_TRUE(mentions(inArch("n <= 1; process begin n <= 2; wait; end process;"), "a concurrent assignment"));
    EXPECT_TRUE(mentions(inArch("col <= red; col <= blue;"), "Signal 'col'"));
}

TEST(Semantic_Drivers, OneProcessIsOneDriverNoMatterHowOftenItAssigns)
{
    EXPECT_EQ(inArch("process begin n <= 1; wait for 1 ns; n <= 2; wait; end process;"), kOk);
    EXPECT_EQ(inArch("process begin if a = '1' then n <= 1; else n <= 2; end if; wait; end process;"), kOk);
}

TEST(Semantic_Drivers, InstanceOutputsAreDrivers)
{
    const std::string message = design("x => p, y => q, s => r, cout => co", "signal k : integer; ");
    EXPECT_EQ(message, kOk);
}

TEST(Semantic_Drivers, ElementsOfAnUnresolvedArrayMayBeDrivenSeparately)
{
    const std::string decls = "type ints is array (0 to 1) of integer; signal arr : ints;";
    EXPECT_EQ(inArch("arr(0) <= 1; arr(1) <= 2;", decls), kOk);
    EXPECT_TRUE(mentions(inArch("arr <= (1, 2); arr <= (3, 4);", decls), "several drivers"));
}

TEST(Semantic_Drivers, DriversOfDifferentArchitecturesDoNotMix)
{
    EXPECT_EQ(analysisError("entity e1 is end e1; architecture a of e1 is signal n : integer; begin n <= 1; end a; "
                            "entity e2 is end e2; architecture a of e2 is signal n : integer; begin n <= 2; end a;"), kOk);
}

TEST(Semantic_Drivers, RecordsOfResolvedFieldsAreResolved)
{
    const std::string decls = "type pt is record x : std_logic; end record; signal r1 : pt;";
    EXPECT_EQ(inArch("r1 <= (x => '1'); r1 <= (x => '0');", decls), kOk);
    const std::string mixed = "type pt is record x : integer; end record; signal r1 : pt;";
    EXPECT_TRUE(mentions(inArch("r1 <= (x => 1); r1 <= (x => 0);", mixed), "several drivers"));
}
