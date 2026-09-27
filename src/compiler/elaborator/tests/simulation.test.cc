// simulation.test.cc — designs elaborated to blueprints and simulated tick by tick (one tick is one femtosecond and one
// VHDL delta cycle): the elaborated logic must behave like the VHDL it comes from.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::Simulation;

namespace
{
    /// A top entity without ports around the given declarations and statements.
    std::string top(const std::string& declarations, const std::string& body)
    {
        return "library ieee; use ieee.std_logic_1164.all; use ieee.numeric_std.all;\n"
               "entity top is end entity top;\n"
               "architecture sim of top is\n" + declarations + "\nbegin\n" + body + "\nend architecture sim;\n";
    }

    /// A clock process that toggles `clk` every `half` femtoseconds, starting low.
    std::string clockProcess(int half = 5)
    {
        return "clock : process begin clk <= '0'; wait for " + std::to_string(half) + " fs; clk <= '1'; wait for "
               + std::to_string(half) + " fs; end process;";
    }
}

// ---- The test project -----------------------------------------------------------------------------

TEST(Simulation_TestProject, TheCounterCountsRisingEdgesAfterTheReset)
{
    const std::string clockEntity = R"(
        ENTITY clock IS PORT (clk_out : OUT STD_LOGIC); END ENTITY clock;
        ARCHITECTURE behavioral OF clock IS
            SIGNAL clk_out_internal : STD_LOGIC := '0';
        BEGIN
            clk_out <= clk_out_internal;
            PROCESS BEGIN
                IF clk_out_internal = '0' THEN clk_out_internal <= '1'; ELSE clk_out_internal <= '0'; END IF;
                WAIT FOR 7 fs;
            END PROCESS;
        END ARCHITECTURE behavioral;)";
    const std::string counterEntity = R"(
        ENTITY counter IS PORT (clk : IN STD_LOGIC; reset : IN STD_LOGIC; count : OUT UNSIGNED(31 DOWNTO 0)); END ENTITY counter;
        ARCHITECTURE behavioral OF counter IS
            SIGNAL count_internal : UNSIGNED(31 DOWNTO 0);
        BEGIN
            PROCESS (clk, reset) BEGIN
                IF reset = '1' THEN count_internal <= x"00000000";
                ELSIF clk'event AND clk = '1' THEN count_internal <= count_internal + 1;
                END IF;
            END PROCESS;
            count <= count_internal;
        END ARCHITECTURE behavioral;)";
    const std::string topEntity = R"(
        ENTITY top IS END ENTITY top;
        ARCHITECTURE behavioral OF top IS
            COMPONENT clock PORT (clk_out : OUT STD_LOGIC); END COMPONENT;
            COMPONENT counter PORT (clk : IN STD_LOGIC; reset : IN STD_LOGIC; count : OUT UNSIGNED(31 DOWNTO 0)); END COMPONENT;
            SIGNAL reset : STD_LOGIC := '1';
            SIGNAL clk : STD_LOGIC;
            SIGNAL count : UNSIGNED(31 DOWNTO 0);
        BEGIN
            clk_inst : clock PORT MAP (clk_out => clk);
            counter_inst : counter PORT MAP (clk => clk, reset => reset, count => count);
            PROCESS BEGIN WAIT FOR 1 fs; reset <= '0'; WAIT; END PROCESS;
        END ARCHITECTURE behavioral;)";

    Simulation sim({ clockEntity, counterEntity, topEntity });

    sim.run(1);
    EXPECT_EQ(sim.bits("reset", 1), "1");
    EXPECT_EQ(sim.bits("clk", 1), "1");     // the clock process ran once at initialization
    EXPECT_EQ(sim.number("count"), 0u);

    sim.run(1);
    EXPECT_EQ(sim.bits("reset", 1), "0");

    // The clock toggles every 7 fs: rising edges at 14, 28, 42 ... fs, each counted in the next tick.
    sim.run(13);    // ticks up to 14
    EXPECT_EQ(sim.number("count"), 0u);
    sim.run(1);     // tick 15
    EXPECT_EQ(sim.number("count"), 1u);
    sim.run(100 - 15);
    EXPECT_EQ(sim.number("count"), 7u);
    EXPECT_EQ(sim.number("counter_inst.count"), 7u);
}

// ---- Combinational logic --------------------------------------------------------------------------

TEST(Simulation_Combinational, GatesAndVectors)
{
    Simulation sim({ top(
        "signal a : std_logic := '1'; signal b : std_logic := '0'; signal y, z : std_logic; "
        "signal u : std_logic_vector(3 downto 0) := \"1100\"; signal v : std_logic_vector(3 downto 0) := \"1010\"; "
        "signal w : std_logic_vector(3 downto 0); signal r : std_logic;",
        "y <= a and not b; z <= a xor b; w <= u nand v; r <= xor u;") });
    sim.run(1);
    EXPECT_EQ(sim.bits("y", 1), "1");
    EXPECT_EQ(sim.bits("z", 1), "1");
    EXPECT_EQ(sim.bits("w", 4), "0111");
    EXPECT_EQ(sim.bits("r", 1), "0");
}

TEST(Simulation_Combinational, ArithmeticFollowsNumericStd)
{
    Simulation sim({ top(
        "signal a : unsigned(7 downto 0) := to_unsigned(200, 8); signal b : unsigned(3 downto 0) := \"0111\"; "
        "signal s : unsigned(7 downto 0); signal p : unsigned(11 downto 0); signal d : unsigned(7 downto 0); "
        "signal n : signed(7 downto 0) := to_signed(-3, 8); signal m : signed(15 downto 0); signal g : boolean;",
        "s <= a + b; p <= a * b; d <= a - 1; m <= n * to_signed(5, 8); g <= a > b;") });
    sim.run(1);
    EXPECT_EQ(sim.number("s"), 207u);
    EXPECT_EQ(sim.number("p"), 1400u);
    EXPECT_EQ(sim.number("d"), 199u);
    EXPECT_EQ(sim.number("m"), static_cast<uint64_t>(static_cast<uint16_t>(-15)));
    EXPECT_EQ(sim.bits("g", 1), "1");
}

TEST(Simulation_Combinational, ConditionalAndSelectedAssignments)
{
    Simulation sim({ top(
        "signal sel : unsigned(1 downto 0) := \"10\"; signal a, b, c : std_logic_vector(3 downto 0); "
        "signal y, z : std_logic_vector(3 downto 0);",
        "a <= x\"1\"; b <= x\"2\"; c <= x\"3\";"
        "y <= a when sel = 0 else b when sel = 2 else c;"
        "with sel select z <= a when \"00\", b when \"01\" | \"10\", c when others;") });
    sim.run(1);
    EXPECT_EQ(sim.number("y"), 2u);
    EXPECT_EQ(sim.number("z"), 2u);
}

TEST(Simulation_Combinational, SlicesConcatenationAndPartialDrivers)
{
    Simulation sim({ top(
        "signal a : std_logic_vector(7 downto 0) := x\"A5\"; signal hi, lo : std_logic_vector(3 downto 0); "
        "signal swapped : std_logic_vector(7 downto 0); signal parts : std_logic_vector(3 downto 0);",
        "hi <= a(7 downto 4); lo <= a(3 downto 0); swapped <= lo & hi;"
        "parts(3 downto 2) <= \"10\"; parts(1) <= a(0); parts(0) <= '0';") });
    sim.run(1);
    EXPECT_EQ(sim.number("hi"), 0xAu);
    EXPECT_EQ(sim.number("lo"), 0x5u);
    EXPECT_EQ(sim.number("swapped"), 0x5Au);
    EXPECT_EQ(sim.bits("parts", 4), "1010");
}

// ---- Processes ------------------------------------------------------------------------------------

TEST(Simulation_Processes, SignalAssignmentsTakeEffectAfterEveryProcessRan)
{
    // A shift register: each flip-flop takes the value its neighbour had before the edge.
    Simulation sim({ top(
        "signal clk : std_logic; signal d : std_logic := '1'; signal q1, q2, q3 : std_logic := '0';",
        clockProcess() + "process (clk) begin if rising_edge(clk) then q1 <= d; q2 <= q1; q3 <= q2; end if; end process;") });

    sim.run(17);    // rising edges at 5 and 15 fs, seen as events one tick later
    EXPECT_EQ(sim.bits("q1", 1), "1");
    EXPECT_EQ(sim.bits("q2", 1), "1");
    EXPECT_EQ(sim.bits("q3", 1), "0");
}

TEST(Simulation_Processes, SwappingThroughDeferredAssignments)
{
    Simulation sim({ top(
        "signal clk : std_logic; signal a : unsigned(3 downto 0) := x\"1\"; signal b : unsigned(3 downto 0) := x\"2\";",
        clockProcess() + "process (clk) begin if rising_edge(clk) then a <= b; b <= a; end if; end process;") });
    sim.run(7);
    EXPECT_EQ(sim.number("a"), 2u);
    EXPECT_EQ(sim.number("b"), 1u);
}

TEST(Simulation_Processes, VariablesAndUnrolledLoops)
{
    // A population count: the variable is updated at once, iteration after iteration.
    Simulation sim({ top(
        "signal v : std_logic_vector(7 downto 0) := \"10110110\"; signal count : integer; signal parity : std_logic;",
        "process (v) variable acc : integer := 0; variable p : std_logic; begin "
        "  acc := 0; p := '0';"
        "  for i in v'range loop if v(i) = '1' then acc := acc + 1; end if; p := p xor v(i); end loop;"
        "  count <= acc; parity <= p;"
        "end process;") });
    sim.run(1);
    EXPECT_EQ(sim.number("count"), 5u);
    EXPECT_EQ(sim.bits("parity", 1), "1");
}

TEST(Simulation_Processes, ExitAndNextInUnrolledLoops)
{
    Simulation sim({ top(
        "signal first, total : integer;",
        "process variable f, t : integer; begin f := -1; t := 0;"
        "  for i in 0 to 9 loop next when i = 2; t := t + i; if i = 6 then f := i; exit; end if; end loop;"
        "  first <= f; total <= t; wait; end process;") });
    sim.run(1);
    EXPECT_EQ(sim.number("first"), 6u);
    EXPECT_EQ(sim.number("total"), 19u);    // 0 + 1 + 3 + 4 + 5 + 6
}

TEST(Simulation_Processes, CaseOverAnEnumerationStateMachine)
{
    Simulation sim({ top(
        "type state_t is (idle, run, done); signal state : state_t := idle; signal clk : std_logic; signal ticks : integer := 0;",
        clockProcess() +
        "process (clk) begin if rising_edge(clk) then "
        "  case state is when idle => state <= run; when run => if ticks = 2 then state <= done; end if; ticks <= ticks + 1; "
        "  when done => null; end case; end if; end process;") });
    sim.run(40);
    EXPECT_EQ(sim.number("state"), 2u);     // done
    EXPECT_EQ(sim.number("ticks"), 3u);
}

TEST(Simulation_Processes, WaitUntilARisingEdge)
{
    Simulation sim({ top(
        "signal clk : std_logic; signal n : unsigned(3 downto 0) := x\"0\";",
        clockProcess(2) + "process begin wait until rising_edge(clk); n <= n + 1; end process;") });
    sim.run(20);    // rising edges at 2, 6, 10, 14, 18 fs
    EXPECT_EQ(sim.number("n"), 5u);
}

TEST(Simulation_Processes, PartialAssignmentsInAProcess)
{
    Simulation sim({ top(
        "signal v : std_logic_vector(3 downto 0) := \"0000\";",
        "process begin v(0) <= '1'; v(2) <= '1'; wait; end process;") });
    sim.run(1);
    EXPECT_EQ(sim.bits("v", 4), "0101");
}

// ---- Hierarchy ------------------------------------------------------------------------------------

TEST(Simulation_Hierarchy, EveryGenericValueGetsItsOwnBlueprint)
{
    const std::string inverter = R"(
        library ieee; use ieee.std_logic_1164.all;
        entity inverter is generic (w : natural := 4); port (a : in std_logic_vector(w - 1 downto 0); y : out std_logic_vector(w - 1 downto 0)); end inverter;
        architecture rtl of inverter is begin y <= not a; end rtl;)";
    const std::string design = top(
        "component inverter generic (w : natural := 4); port (a : in std_logic_vector(w - 1 downto 0); y : out std_logic_vector(w - 1 downto 0)); end component;"
        "signal a4, y4, b4, z4 : std_logic_vector(3 downto 0); signal a8, y8 : std_logic_vector(7 downto 0);",
        "a4 <= \"0011\"; b4 <= \"0101\"; a8 <= x\"0F\";"
        "u1 : inverter port map (a => a4, y => y4);"
        "u2 : inverter generic map (w => 8) port map (a => a8, y => y8);"
        "u3 : inverter generic map (4) port map (b4, z4);");

    Simulation sim({ inverter, design });
    sim.run(1);
    EXPECT_EQ(sim.bits("y4", 4), "1100");
    EXPECT_EQ(sim.bits("z4", 4), "1010");
    EXPECT_EQ(sim.number("y8"), 0xF0u);
    EXPECT_EQ(sim.elaborated.blueprints.size(), 3u);    // inverter(4), inverter(8) and top
}

TEST(Simulation_Hierarchy, OpenPortsAndPortDefaults)
{
    const std::string leaf = R"(
        library ieee; use ieee.std_logic_1164.all;
        entity leaf is port (en : in std_logic := '1'; d : in std_logic; q : out std_logic; spare : out std_logic); end leaf;
        architecture rtl of leaf is begin q <= d and en; spare <= d; end rtl;)";
    const std::string design = top(
        "component leaf port (en : in std_logic := '1'; d : in std_logic; q : out std_logic; spare : out std_logic); end component;"
        "signal q : std_logic;",
        "u : leaf port map (d => '1', q => q, spare => open);");

    Simulation sim({ leaf, design });
    sim.run(1);
    EXPECT_EQ(sim.bits("q", 1), "1");
}

// ---- Integers, booleans and enumerations ----------------------------------------------------------

TEST(Simulation_Scalars, IntegerArithmeticAndComparisons)
{
    Simulation sim({ top(
        "signal a : integer := -7; signal b : integer := 3; signal sum, diff, product, negated, magnitude : integer; "
        "signal less, equal : boolean; signal small : natural range 0 to 15 := 12; signal next_small : natural;",
        "sum <= a + b; diff <= a - b; product <= a * b; negated <= -a; magnitude <= abs a;"
        "less <= a < b; equal <= a = -7; next_small <= small + 1;") });
    sim.run(1);
    const auto asInt = [&](const char* name) { return static_cast<int32_t>(static_cast<uint32_t>(sim.number(name))); };
    EXPECT_EQ(asInt("sum"), -4);
    EXPECT_EQ(asInt("diff"), -10);
    EXPECT_EQ(asInt("product"), -21);
    EXPECT_EQ(asInt("negated"), 7);
    EXPECT_EQ(asInt("magnitude"), 7);
    EXPECT_EQ(sim.bits("less", 1), "1");
    EXPECT_EQ(sim.bits("equal", 1), "1");
    EXPECT_EQ(asInt("next_small"), 13);
}

TEST(Simulation_Scalars, ConversionsBetweenIntegersAndVectors)
{
    Simulation sim({ top(
        "signal n : integer := -2; signal u : unsigned(7 downto 0) := x\"F3\"; signal s : signed(7 downto 0) := x\"F3\"; "
        "signal from_u, from_s : integer; signal to_u : unsigned(3 downto 0); signal to_s : signed(11 downto 0); "
        "signal shrunk : signed(3 downto 0); signal grown : unsigned(11 downto 0); signal raw : std_logic_vector(7 downto 0);",
        "from_u <= to_integer(u); from_s <= to_integer(s); to_u <= to_unsigned(9, 4); to_s <= to_signed(n, 12);"
        "shrunk <= resize(s, 4); grown <= resize(u, 12); raw <= std_logic_vector(u);") });
    sim.run(1);
    EXPECT_EQ(sim.number("from_u"), 0xF3u);
    EXPECT_EQ(static_cast<int32_t>(static_cast<uint32_t>(sim.number("from_s"))), -13);
    EXPECT_EQ(sim.number("to_u"), 9u);
    EXPECT_EQ(sim.number("to_s"), 0xFFEu);
    EXPECT_EQ(sim.bits("shrunk", 4), "1011");   // numeric_std keeps the sign bit
    EXPECT_EQ(sim.number("grown"), 0x0F3u);
    EXPECT_EQ(sim.number("raw"), 0xF3u);
}

TEST(Simulation_Scalars, VectorsComparedWithIntegers)
{
    Simulation sim({ top(
        "signal u : unsigned(7 downto 0) := to_unsigned(200, 8); signal s : signed(7 downto 0) := to_signed(-5, 8); "
        "signal n : integer := 150; signal a, b, c, d : boolean;",
        "a <= u > 199; b <= u < n; c <= s < -1; d <= s = -5;") });
    sim.run(1);
    EXPECT_EQ(sim.bits("a", 1), "1");
    EXPECT_EQ(sim.bits("b", 1), "0");
    EXPECT_EQ(sim.bits("c", 1), "1");
    EXPECT_EQ(sim.bits("d", 1), "1");
}

TEST(Simulation_Scalars, ShiftsAndRotations)
{
    Simulation sim({ top(
        "signal v : std_logic_vector(7 downto 0) := \"10010110\"; signal s : signed(7 downto 0) := \"10010110\"; "
        "signal k : integer := 3; signal a, b, c, d, e, f : std_logic_vector(7 downto 0); signal g, h : signed(7 downto 0);",
        "a <= v sll 2; b <= v srl k; c <= v rol 3; d <= v ror 1; e <= v sll -1; f <= v sll 9;"
        "g <= shift_right(s, 2); h <= s sra k;") });
    sim.run(1);
    EXPECT_EQ(sim.bits("a", 8), "01011000");
    EXPECT_EQ(sim.bits("b", 8), "00010010");
    EXPECT_EQ(sim.bits("c", 8), "10110100");
    EXPECT_EQ(sim.bits("d", 8), "01001011");
    EXPECT_EQ(sim.bits("e", 8), "01001011");
    EXPECT_EQ(sim.bits("f", 8), "00000000");
    EXPECT_EQ(sim.bits("g", 8), "11100101");
    EXPECT_EQ(sim.bits("h", 8), "11110010");
}

TEST(Simulation_Scalars, AggregatesWithSignalElements)
{
    Simulation sim({ top(
        "signal a : std_logic := '1'; signal b : std_logic := '0'; signal v : std_logic_vector(3 downto 0); "
        "signal w : std_logic_vector(0 to 3);",
        "v <= (3 => a, 1 => a, others => b); w <= (a, b, '1', others => '0');") });
    sim.run(1);
    EXPECT_EQ(sim.bits("v", 4), "1010");
    EXPECT_EQ(sim.bits("w", 4), "1010");
}

// ---- Matching -------------------------------------------------------------------------------------

TEST(Simulation_Matching, DontCareElementsMatchAnything)
{
    Simulation sim({ top(
        "signal v : std_logic_vector(3 downto 0) := \"1011\"; signal m1, m2 : std_logic; signal code : unsigned(1 downto 0);",
        "m1 <= v ?= \"1-11\"; m2 <= v ?= \"0---\";"
        "with v select? code <= \"01\" when \"0---\", \"10\" when \"1-1-\", \"11\" when others;") });
    sim.run(1);
    EXPECT_EQ(sim.bits("m1", 1), "1");
    EXPECT_EQ(sim.bits("m2", 1), "0");
    EXPECT_EQ(sim.number("code"), 2u);
}

// ---- Processes that keep a value ------------------------------------------------------------------

TEST(Simulation_Latches, AConditionalAssignmentWithoutElseKeepsItsValue)
{
    Simulation sim({ top(
        "signal en : std_logic := '1'; signal d : std_logic := '1'; signal q : std_logic := '0';",
        "q <= d when en = '1';"
        "stimulus : process begin wait for 3 fs; en <= '0'; wait for 1 fs; d <= '0'; wait; end process;") });
    sim.run(2);
    EXPECT_EQ(sim.bits("q", 1), "1");
    sim.run(6);
    EXPECT_EQ(sim.bits("d", 1), "0");
    EXPECT_EQ(sim.bits("q", 1), "1") << "the latch is closed";
}

TEST(Simulation_Latches, ARisingEdgeInAConcurrentAssignmentIsARegister)
{
    Simulation sim({ top(
        "signal clk : std_logic; signal d : unsigned(3 downto 0) := x\"0\"; signal q : unsigned(3 downto 0) := x\"0\";",
        clockProcess() + "q <= d when rising_edge(clk); d <= d + 1 when rising_edge(clk);") });
    sim.run(27);    // edges seen at 6, 16 and 26 fs
    EXPECT_EQ(sim.number("d"), 3u);
    EXPECT_EQ(sim.number("q"), 2u);
}

TEST(Simulation_Latches, ProcessAllReadsEverySignalItUses)
{
    Simulation sim({ top(
        "signal a : std_logic := '0'; signal b : std_logic := '1'; signal y : std_logic;",
        "process (all) begin if a = '1' then y <= b; else y <= not b; end if; end process;"
        "process begin wait for 2 fs; a <= '1'; wait for 2 fs; b <= '0'; wait; end process;") });
    sim.run(1);
    EXPECT_EQ(sim.bits("y", 1), "0");
    sim.run(3);
    EXPECT_EQ(sim.bits("y", 1), "1");
    sim.run(3);
    EXPECT_EQ(sim.bits("y", 1), "0");
}

// ---- Waits ----------------------------------------------------------------------------------------

TEST(Simulation_Waits, WaitOnUntilAndTimeouts)
{
    Simulation sim({ top(
        "signal go : std_logic := '0'; signal step : integer := 0;",
        "process begin wait until go = '1' for 20 fs; step <= 1; wait on go; step <= 2; wait for 0 fs; step <= 3; wait; end process;"
        "process begin wait for 5 fs; go <= '1'; wait for 5 fs; go <= '0'; wait; end process;") });
    sim.run(7);     // go rises at 5 fs: the wait ends at 6 fs
    EXPECT_EQ(sim.number("step"), 1u);
    sim.run(5);     // go falls at 10 fs: the `wait on` ends at 11 fs
    EXPECT_EQ(sim.number("step"), 2u);
    sim.run(1);     // `wait for 0 fs` lasts one delta, one tick
    EXPECT_EQ(sim.number("step"), 3u);
}

TEST(Simulation_Waits, AWaitUntilTimesOut)
{
    Simulation sim({ top(
        "signal go : std_logic := '0'; signal done : boolean := false;",
        "process begin wait until go = '1' for 10 fs; done <= true; wait; end process;") });
    sim.run(10);
    EXPECT_EQ(sim.bits("done", 1), "0");
    sim.run(2);
    EXPECT_EQ(sim.bits("done", 1), "1");
}

// ---- Hierarchy ------------------------------------------------------------------------------------

TEST(Simulation_Hierarchy, OutputsConnectedToPartsOfASignal)
{
    const std::string bit = R"(
        library ieee; use ieee.std_logic_1164.all;
        entity bit_driver is generic (value : std_logic := '0'); port (q : out std_logic); end bit_driver;
        architecture rtl of bit_driver is begin q <= value; end rtl;)";
    const std::string design = top(
        "component bit_driver generic (value : std_logic := '0'); port (q : out std_logic); end component;"
        "signal bus_v : std_logic_vector(2 downto 0);",
        "u0 : bit_driver generic map ('1') port map (q => bus_v(0));"
        "u1 : bit_driver port map (q => bus_v(1));"
        "u2 : bit_driver generic map (value => '1') port map (q => bus_v(2));");

    Simulation sim({ bit, design });
    sim.run(1);
    EXPECT_EQ(sim.bits("bus_v", 3), "101");
}

TEST(Simulation_Hierarchy, AProcessDrivesAnOutputPortThroughItsInstance)
{
    const std::string counter = R"(
        library ieee; use ieee.std_logic_1164.all; use ieee.numeric_std.all;
        entity counter is generic (w : natural := 4); port (clk : in std_logic; q : out unsigned(w - 1 downto 0) := (others => '0')); end counter;
        architecture rtl of counter is begin
            process (clk) begin if rising_edge(clk) then q <= q + 1; end if; end process;
        end rtl;)";
    const std::string design = top(
        "component counter generic (w : natural := 4); port (clk : in std_logic; q : out unsigned(w - 1 downto 0)); end component;"
        "signal clk : std_logic; signal small : unsigned(3 downto 0); signal wide : unsigned(9 downto 0);",
        clockProcess() + "c4 : counter port map (clk, small); c10 : counter generic map (10) port map (clk => clk, q => wide);");

    Simulation sim({ counter, design });
    sim.run(40 * 10 + 7);   // 41 edges (5, 15, ... 405 fs)
    EXPECT_EQ(sim.number("small"), 41u % 16u);
    EXPECT_EQ(sim.number("wide"), 41u);
    EXPECT_EQ(sim.number("c10.q"), 41u);
}

TEST(Simulation_Processes, SelectedAssignmentsInAProcess)
{
    Simulation sim({ top(
        "signal sel : integer := 2; signal y : std_logic_vector(1 downto 0) := \"00\";",
        "process begin with sel select y <= \"01\" when 1, \"10\" when 2 to 3, unaffected when others; wait; end process;") });
    sim.run(1);
    EXPECT_EQ(sim.bits("y", 2), "10");
}
