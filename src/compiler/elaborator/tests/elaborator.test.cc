// elaborator.test.cc — elaboration of a linked design into blueprints: choosing the top entity and the architectures,
// giving generics their values, the ports, signals and symbols of each blueprint, the rules only an instance can check,
// and every construct that is not lowered yet, which is rejected with a message naming it.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using namespace Pulse::Parser;
using TestUtil::Simulation;
using TestUtil::elaborationError;

namespace
{
    constexpr const char* kOk = "<no error>";

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    const std::string kContext = "library ieee; use ieee.std_logic_1164.all; use ieee.numeric_std.all;\n";

    /// A top entity without ports around the given declarations and statements.
    std::string top(const std::string& declarations, const std::string& body)
    {
        return kContext + "entity top is end entity top;\narchitecture sim of top is\n" + declarations + "\nbegin\n" + body
               + "\nend architecture sim;\n";
    }

    /// The error of elaborating a top entity with these declarations and statements.
    std::string errorOf(const std::string& declarations, const std::string& body)
    {
        return elaborationError({ top(declarations, body) });
    }

    const Pulse::Engine::Blueprint& topBlueprint(const Simulation& sim)
    {
        return *sim.elaborated.top;
    }
}

// ===========================================================================
// 1. THE TOP ENTITY AND ITS ARCHITECTURE
// ===========================================================================

TEST(Elaborator_Top, AMissingTopEntityIsReported)
{
    const std::string message = elaborationError({ top("", "") }, "nothing");
    EXPECT_TRUE(mentions(message, "Top-level entity 'nothing' not found")) << message;
}

TEST(Elaborator_Top, TheMostRecentlyAnalyzedArchitectureIsTheDefault)
{
    const std::string design = kContext +
        "entity top is end top;"
        "architecture first of top is signal s : std_logic; begin s <= '0'; end first;"
        "architecture second of top is signal s : std_logic; begin s <= '1'; end second;";

    Simulation latest({ design });
    latest.run(1);
    EXPECT_EQ(latest.bits("s", 1), "1");

    Simulation chosen({ design }, "top", "first");
    chosen.run(1);
    EXPECT_EQ(chosen.bits("s", 1), "0");

    EXPECT_TRUE(mentions(elaborationError({ design }, "top") , kOk));
    try
    {
        Simulation missing({ design }, "top", "third");
        ADD_FAILURE() << "no error for a missing architecture";
    }
    catch (const compiler_error& e)
    {
        EXPECT_TRUE(mentions(e.what(), "has no architecture 'third'")) << e.what();
    }
}

TEST(Elaborator_Top, TopGenericsTakeTheirDefaults)
{
    const std::string design = kContext +
        "entity top is generic (n : natural := 5); end top;"
        "architecture sim of top is signal s : unsigned(n - 1 downto 0) := to_unsigned(n, n); begin end sim;";
    Simulation sim({ design });
    EXPECT_EQ(sim.number("s"), 5u);
    EXPECT_EQ(sim.elaborated.top->wires.at("s").width, 5);

    const std::string noDefault = kContext + "entity top is generic (n : natural); end top; architecture sim of top is begin end sim;";
    const std::string message = elaborationError({ noDefault });
    EXPECT_TRUE(mentions(message, "Generic 'n' of entity 'top' has no value")) << message;
}

TEST(Elaborator_Top, PortsOfTheTopBecomeWiresOfTheRootSubgraph)
{
    const std::string design = kContext +
        "entity top is port (a : in std_logic := '1'; b : in unsigned(3 downto 0) := x\"9\"; y : out std_logic; z : out unsigned(3 downto 0)); end top;"
        "architecture sim of top is begin y <= not a; z <= b + 1; end sim;";
    Simulation sim({ design });
    sim.run(1);
    EXPECT_EQ(sim.bits("a", 1), "1");
    EXPECT_EQ(sim.bits("y", 1), "0");
    EXPECT_EQ(sim.number("z"), 10u);
}

TEST(Elaborator_Top, NineValuedLogicIsNotSupportedYet)
{
    DesignLibrary library;
    std::vector<ASTRoot> files = TestUtil::analyzeFiles({ top("", "") }, library);
    Linker linker(library);
    linker.addAST(std::move(files.front()));
    const ASTRoot design = linker.link();

    ElaborationOptions options;
    options.logic = LogicMode::Vector;
    try
    {
        elaborate(library, design, options);
        ADD_FAILURE() << "no error without -Ologic";
    }
    catch (const elaboration_error& e)
    {
        EXPECT_TRUE(mentions(e.what(), "-Ologic")) << e.what();
    }
}

// ===========================================================================
// 2. WHAT A BLUEPRINT HOLDS
// ===========================================================================

TEST(Elaborator_Blueprint, SignalsHaveTheWidthAndInitialValueOfTheirType)
{
    Simulation sim({ top(
        "type state_t is (idle, a, b, c, d); signal st : state_t; signal n : integer; signal k : natural range 0 to 9 := 7; "
        "signal f : boolean := true; signal l : std_logic; signal v : std_logic_vector(0 to 5) := \"01ZX10\";", "") });
    const auto& bp = topBlueprint(sim);

    EXPECT_EQ(bp.wires.at("st").width, 3);                 // five literals
    EXPECT_EQ(bp.wires.at("n").width, 32);
    EXPECT_EQ(bp.wires.at("n").defaultValue, Pulse::Engine::LogicVector(0x80000000u));    // integer'left
    EXPECT_EQ(bp.wires.at("k").defaultValue, Pulse::Engine::LogicVector(7));
    EXPECT_EQ(bp.wires.at("f").defaultValue, Pulse::Engine::LogicVector(1));
    EXPECT_EQ(bp.wires.at("l").defaultValue.str(1), "X");  // 'U'
    EXPECT_EQ(bp.wires.at("v").defaultValue.str(6), "01ZX10");
}

TEST(Elaborator_Blueprint, TheSymbolTableDescribesEverySignal)
{
    Simulation sim({ top(
        "type state_t is (idle, busy); signal st : state_t; signal n : integer; signal f : boolean; signal l : std_logic; "
        "signal v : std_logic_vector(0 to 3); signal u : unsigned(7 downto 0);",
        "process variable hidden : integer; begin wait; end process;") });
    const auto& symbols = topBlueprint(sim).symbols;

    ASSERT_NE(symbols.find("st"), nullptr);
    EXPECT_EQ(symbols.find("st")->format, Pulse::Engine::DisplayFormat::Enumeration);
    EXPECT_EQ(symbols.find("st")->literals, (std::vector<std::string>{ "idle", "busy" }));
    EXPECT_EQ(symbols.find("n")->format, Pulse::Engine::DisplayFormat::SignedInteger);
    EXPECT_EQ(symbols.find("f")->format, Pulse::Engine::DisplayFormat::Boolean);
    EXPECT_EQ(symbols.find("l")->format, Pulse::Engine::DisplayFormat::Bit);
    EXPECT_EQ(symbols.find("v")->format, Pulse::Engine::DisplayFormat::Hex);
    EXPECT_TRUE(symbols.find("v")->ascending);
    EXPECT_EQ(symbols.find("v")->left, 0);
    EXPECT_EQ(symbols.find("u")->left, 7);

    for (const auto& [name, symbol] : symbols.signals)
        EXPECT_NE(name.front(), '$') << "hidden wires are not described";
}

TEST(Elaborator_Blueprint, HiddenWiresAreNotRecordedInSnapshots)
{
    Simulation sim({ top("signal a, b, y : std_logic;", "y <= (a and b) or not a;") });
    const auto snapshot = sim.graph->takeSnapshot();
    for (const auto& [name, value] : snapshot.wires)
        EXPECT_NE(name.front(), '$');
    EXPECT_EQ(snapshot.wires.size(), 3u);
}

TEST(Elaborator_Blueprint, InstancesWithTheSameGenericValuesShareABlueprint)
{
    const std::string leaf = kContext +
        "entity leaf is generic (w : natural := 2); port (a : in std_logic_vector(w - 1 downto 0); y : out std_logic_vector(w - 1 downto 0)); end leaf;"
        "architecture rtl of leaf is begin y <= a; end rtl;";
    const std::string design = top(
        "component leaf generic (w : natural := 2); port (a : in std_logic_vector(w - 1 downto 0); y : out std_logic_vector(w - 1 downto 0)); end component;"
        "signal a, y1, y2 : std_logic_vector(1 downto 0);",
        "u1 : leaf port map (a, y1); u2 : leaf generic map (w => 2) port map (a, y2);");

    Simulation sim({ leaf, design });
    EXPECT_EQ(sim.elaborated.blueprints.size(), 2u);
    const auto& u1 = static_cast<const Pulse::Engine::SubgraphInstance&>(*topBlueprint(sim).components.at("u1"));
    const auto& u2 = static_cast<const Pulse::Engine::SubgraphInstance&>(*topBlueprint(sim).components.at("u2"));
    EXPECT_EQ(u1.bp, u2.bp);
}

// ===========================================================================
// 3. RULES ONLY AN INSTANCE CAN CHECK
// ===========================================================================

TEST(Elaborator_Rules, AnEntityCannotInstantiateItself)
{
    const std::string design = kContext +
        "entity top is end top;"
        "architecture sim of top is component top end component; begin u : top; end sim;";
    const std::string message = elaborationError({ design });
    EXPECT_TRUE(mentions(message, "instantiates itself")) << message;
}

TEST(Elaborator_Rules, ASignalAssignedInAProcessHasNoOtherDriver)
{
    std::string message = errorOf("signal s : std_logic;", "p1 : process begin s <= '1'; wait; end process; s <= '0';");
    EXPECT_TRUE(mentions(message, "Signal 's' is assigned by process 'p1' and also driven by the concurrent assignment")) << message;
    EXPECT_TRUE(mentions(message, "not supported yet")) << message;

    message = errorOf("signal s : std_logic;", "p1 : process begin s <= '1'; wait; end process; p2 : process begin s <= '0'; wait; end process;");
    EXPECT_TRUE(mentions(message, "process 'p2'")) << message;

    // Two concurrent drivers of a resolved signal are resolved by the engine.
    EXPECT_EQ(errorOf("signal s : std_logic;", "s <= '1'; s <= 'Z';"), kOk);
}

TEST(Elaborator_Rules, APortAProcessDrivesCannotShareItsActual)
{
    const std::string leaf = kContext +
        "entity leaf is port (q : out std_logic); end leaf;"
        "architecture rtl of leaf is begin process begin q <= '1'; wait; end process; end rtl;";
    const std::string design = top("component leaf port (q : out std_logic); end component; signal s : std_logic;",
                                   "u : leaf port map (q => s); s <= '0';");
    const std::string message = elaborationError({ leaf, design });
    EXPECT_TRUE(mentions(message, "instance 'u' (port 'q')")) << message;
}

TEST(Elaborator_Rules, WidthsThatDependOnGenericsMustAgree)
{
    const std::string leaf = kContext +
        "entity leaf is generic (w : natural := 4); port (a : in std_logic_vector(w - 1 downto 0)); end leaf;"
        "architecture rtl of leaf is begin end rtl;";
    const std::string design = top("component leaf generic (w : natural := 4); port (a : in std_logic_vector(w - 1 downto 0)); end component;"
                                   "signal s : std_logic_vector(3 downto 0);",
                                   "u : leaf generic map (w => 8) port map (a => s);");
    const std::string message = elaborationError({ leaf, design });
    EXPECT_TRUE(mentions(message, "has 8 bits")) << message;
}

// ===========================================================================
// 4. NOT SUPPORTED YET
// ===========================================================================

TEST(Elaborator_Unsupported, TypesThatAreNotLogicVectors)
{
    EXPECT_TRUE(mentions(errorOf("type pair is record a, b : std_logic; end record; signal p : pair;", ""), "Records is not supported yet"));
    EXPECT_TRUE(mentions(errorOf("type mem is array (0 to 3) of std_logic_vector(7 downto 0); signal m : mem;", ""),
                         "Arrays other than one-dimensional arrays of std_logic"));
    EXPECT_TRUE(mentions(errorOf("signal r : real;", ""), "real types"));
    EXPECT_TRUE(mentions(errorOf("signal t : time;", ""), "physical types"));
    EXPECT_TRUE(mentions(errorOf("signal w : std_logic_vector(64 downto 0);", ""), "Vectors wider than 64 elements"));
}

TEST(Elaborator_Unsupported, StatementsThatAreNotLoweredYet)
{
    EXPECT_TRUE(mentions(errorOf("", "assert false;"), "The assert statement is not supported yet"));
    EXPECT_TRUE(mentions(errorOf(TestUtil::stringTypes(), "process begin report \"x\"; wait; end process;"),
                         "The report statement is not supported yet"));
    EXPECT_TRUE(mentions(errorOf("signal n : integer;", "process begin while n < 3 loop n <= n + 1; end loop; wait; end process;"),
                         "'while' loop"));
    EXPECT_TRUE(mentions(errorOf("procedure p is begin end procedure;", "process begin p; wait; end process;"), "Calling a procedure"));
}

TEST(Elaborator_Unsupported, ExpressionsThatAreNotLoweredYet)
{
    EXPECT_TRUE(mentions(errorOf("function f(x : std_logic) return std_logic is begin return x; end function; signal a, y : std_logic;",
                                 "y <= f(a);"), "Calling a function declared in the design ('f')"));
    EXPECT_TRUE(mentions(errorOf("signal v : std_logic_vector(3 downto 0); signal i : integer; signal y : std_logic;", "y <= v(i);"),
                         "Indexing a vector with a value that is not known at elaboration time"));
    EXPECT_TRUE(mentions(errorOf("signal a, b, y : unsigned(3 downto 0);", "y <= a / b;"), "'/' on values that are not known"));
    EXPECT_TRUE(mentions(errorOf("signal a, y : unsigned(3 downto 0); signal n : integer;", "y <= a sla n;"), "'sla'"));
    EXPECT_TRUE(mentions(errorOf("signal a : std_logic; alias b : std_logic is a;", ""), "Aliases"));
}

TEST(Elaborator_Unsupported, ConstructsOfPortMaps)
{
    const std::string leaf = kContext + "entity leaf is port (a : in std_logic_vector(1 downto 0)); end leaf; architecture rtl of leaf is begin end rtl;";
    const std::string design = top("component leaf port (a : in std_logic_vector(1 downto 0)); end component; signal x, y : std_logic;",
                                   "u : leaf port map (a(0) => x, a(1) => y);");
    const std::string message = elaborationError({ leaf, design });
    EXPECT_TRUE(mentions(message, "Associating a part of a port")) << message;
}

// ===========================================================================
// 5. VALUES KNOWN AT ELABORATION TIME
// ===========================================================================

TEST(Elaborator_Values, ConstantsAndGenericsAreFolded)
{
    const std::string design = kContext +
        "entity top is generic (w : natural := 6); end top;"
        "architecture sim of top is"
        "    constant half : natural := w / 2;"
        "    constant mask : unsigned(w - 1 downto 0) := resize(to_unsigned(half, 3) & \"1\", w);"
        "    constant pattern : std_logic_vector(7 downto 0) := (7 => '1', 3 downto 1 => '1', others => '0');"
        "    signal a : unsigned(w - 1 downto 0) := mask;"
        "    signal b : std_logic_vector(7 downto 0) := pattern;"
        "    signal c : std_logic := pattern(3);"
        "    signal d : integer := half ** 2 - 1;"
        "begin end sim;";
    Simulation sim({ design });
    EXPECT_EQ(sim.number("a"), 7u);         // "011" & "1"
    EXPECT_EQ(sim.number("b"), 0x8Eu);
    EXPECT_EQ(sim.bits("c", 1), "1");
    EXPECT_EQ(sim.number("d"), 8u);
}

TEST(Elaborator_Values, AValueThatIsNotStaticIsReportedWhereOneIsNeeded)
{
    const std::string message = errorOf("signal n : integer := 3;", "process begin wait for n * 1 fs; end process;");
    EXPECT_TRUE(mentions(message, "must be known at elaboration time")) << message;
}
