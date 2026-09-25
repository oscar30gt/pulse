// analyzer_associations.test.cc — port maps in every form: named, positional, `open`, element-by-element associations of one
// port, and conversions on the formal or on the actual.

#include <gtest/gtest.h>
#include <string>

#include "test_helpers.h"

using TestUtil::analysisError;

namespace
{
    constexpr const char* kOk = "<no error>";

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    const std::string kPorts =
        "a : in std_logic; b : in std_logic; y : out std_logic; "
        "v : in std_logic_vector(3 downto 0) := \"0000\"; q : out std_logic_vector(3 downto 0); u : out unsigned(3 downto 0); "
        "n : out integer; io : inout std_logic; pv : in std_logic_vector(3 downto 0)";

    const std::string kEntity = "entity dev is port (" + kPorts + "); end dev; architecture r of dev is begin end r; ";

    const std::string kComponent = "component dev is port (" + kPorts + "); end component; ";

    const std::string kSignals =
        "signal sa, sb, sy, sio : std_logic; signal sv, sq : std_logic_vector(3 downto 0); signal su : unsigned(3 downto 0); signal sn : integer; "
        "signal sv2 : std_logic_vector(1 downto 0); ";

    /// Instantiates `dev` with the given port map, declaring `extra` in the architecture. The input `pv` is connected to a
    /// signal unless the map names it, so that a test only fails for the reason it is about.
    std::string map(const std::string& portMap, const std::string& extra = "")
    {
        const bool mentionsPv = portMap.find("pv") != std::string::npos;
        const std::string full = mentionsPv ? portMap : portMap + (portMap.empty() ? "" : ", ") + "pv => sv";
        return analysisError(kEntity + "entity top is end top; architecture t of top is " + kComponent + kSignals + extra + " begin "
                             "u1 : dev port map (" + full + "); end t;");
    }

    const std::string kFull = "a => sa, b => sb, y => sy, v => sv, q => sq, u => su, n => sn, io => sio";
}

// ===========================================================================
// 1. NAMED AND POSITIONAL
// ===========================================================================

TEST(Associations_Ports, NamedInAnyOrder)
{
    EXPECT_EQ(map(kFull), kOk);
    EXPECT_EQ(map("io => sio, n => sn, u => su, q => sq, v => sv, y => sy, b => sb, a => sa"), kOk);
}

TEST(Associations_Ports, PositionalInDeclarationOrder)
{
    EXPECT_EQ(map("sa, sb, sy, sv, sq, su, sn, sio"), kOk);
    EXPECT_EQ(analysisError(kEntity + "entity top is end top; architecture t of top is " + kComponent + kSignals +
                            " begin u1 : dev port map (sa, sb, sy, sv, sq, su, sn, sio, sv); end t;"), kOk) << "all nine ports, positionally";
}

TEST(Associations_Ports, PositionalThenNamed)
{
    EXPECT_EQ(map("sa, sb, sy, v => sv, q => sq, u => su, n => sn, io => sio"), kOk);
}

TEST(Associations_Ports, PositionalActualsAreTypeChecked)
{
    EXPECT_EQ(map("sa, sb, sy, sq, sv, su, sn, sio"), kOk) << "v and q have the same type, so swapping two vector signals is fine";
    EXPECT_TRUE(mentions(map("sa, sb, sv, sv, sq, su, sn, sio"), "Type mismatch"));
}

TEST(Associations_Ports, PositionalAfterNamedIsRejected)
{
    EXPECT_TRUE(mentions(map("a => sa, sb, sy, sv, sq, su, sn, sio"), "positional association cannot follow a named association"));
}

TEST(Associations_Ports, TooManyPositionalActuals)
{
    const std::string message = analysisError(kEntity + "entity top is end top; architecture t of top is " + kComponent + kSignals +
                                              " begin u1 : dev port map (sa, sb, sy, sv, sq, su, sn, sio, sv, sa); end t;");
    EXPECT_TRUE(mentions(message, "Too many associations for instance 'u1': it has only 9 ports")) << message;
}

TEST(Associations_Ports, UnknownPortListsTheAvailableOnes)
{
    const std::string message = map(kFull + ", zz => sa");
    EXPECT_TRUE(mentions(message, "has no port named 'zz'")) << message;
    EXPECT_TRUE(mentions(message, "ports: a, b, y, v, q, u, n, io, pv")) << message;
}

TEST(Associations_Ports, APortIsConnectedOnce)
{
    EXPECT_TRUE(mentions(map(kFull + ", a => sb"), "Port 'a' of instance 'u1' is connected twice"));
    EXPECT_TRUE(mentions(map("sa, a => sb, b => sb, y => sy, v => sv, q => sq, u => su, n => sn, io => sio"), "connected twice"));
}

TEST(Associations_Ports, InputsMustBeConnected)
{
    EXPECT_TRUE(mentions(map("b => sb, y => sy, v => sv, q => sq, u => su, n => sn, io => sio"), "Input port 'a' of instance 'u1' is not connected"));
    EXPECT_TRUE(mentions(analysisError(kEntity + "entity top is end top; architecture t of top is " + kComponent + kSignals +
                                       " begin u1 : dev port map (a => sa, b => sb); end t;"), "Input port 'pv' of instance 'u1' is not connected"));
    EXPECT_EQ(map("a => sa, b => sb"), kOk) << "outputs, inouts and inputs with a default may stay unconnected";
}

// ===========================================================================
// 2. OPEN
// ===========================================================================

TEST(Associations_Open, OutputsAndInoutsMayBeOpen)
{
    EXPECT_EQ(map("a => sa, b => sb, y => open, v => sv, q => open, u => open, n => open, io => open"), kOk);
}

TEST(Associations_Open, InputsMayNotBeOpen)
{
    EXPECT_TRUE(mentions(map("a => open, b => sb"), "Port 'a' of instance 'u1' cannot be left open"));
}

TEST(Associations_Open, PositionalOpen)
{
    EXPECT_EQ(analysisError(kEntity + "entity top is end top; architecture t of top is " + kComponent + kSignals +
                            " begin u1 : dev port map (sa, sb, open, sv, open, open, open, open, sv); end t;"), kOk);
    EXPECT_TRUE(mentions(map("open, sb"), "cannot be left open"));
}

// ===========================================================================
// 3. WHAT MAY BE CONNECTED
// ===========================================================================

TEST(Associations_Actuals, InputsTakeAnyExpressionOfTheRightType)
{
    EXPECT_EQ(map("a => sa and sb, b => '1', y => sy, v => \"0101\", q => sq, u => su, n => sn, io => sio"), kOk);
    EXPECT_EQ(map("a => sa, b => sb, v => sv(3 downto 0) and sq"), kOk);
    EXPECT_EQ(map("a => sa, b => sb, v => (others => '0')"), kOk);
}

TEST(Associations_Actuals, InputActualsAreTypeAndLengthChecked)
{
    EXPECT_TRUE(mentions(map("a => sn, b => sb"), "Type mismatch"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, v => sv2"), "Length mismatch"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, v => \"01\""), "element"));
    EXPECT_TRUE(mentions(map("a => nothing, b => sb"), "'nothing' is not declared"));
}

TEST(Associations_Actuals, OutputsNeedAWritableActualOfTheRightType)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, y => '1'"), "The target of the actual of port 'y' of instance 'u1' must be a signal or a port"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, y => sn"), "Type mismatch"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, y => k", "constant k : std_logic := '1'; "), "constant and cannot be assigned"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, q => sv2"), "Length mismatch"));
}

TEST(Associations_Actuals, InoutsAreCheckedBothWays)
{
    EXPECT_EQ(map("a => sa, b => sb, io => sio"), kOk);
    EXPECT_TRUE(mentions(map("a => sa, b => sb, io => sn"), "Type mismatch"));
}

TEST(Associations_Actuals, AnOutputDrivesItsSignal)
{
    const std::string twice = analysisError(kEntity + "entity top is end top; architecture t of top is " + kComponent + kSignals + " signal k : integer; begin "
                                            "u1 : dev port map (a => sa, b => sb, pv => sv, n => k); u2 : dev port map (a => sa, b => sb, pv => sv, n => k); end t;");
    EXPECT_TRUE(mentions(twice, "cannot have several drivers")) << twice;
    EXPECT_TRUE(mentions(twice, "instance 'u1', instance 'u2'")) << twice;

    const std::string resolved = analysisError(kEntity + "entity top is end top; architecture t of top is " + kComponent + kSignals + " begin "
                                               "u1 : dev port map (a => sa, b => sb, pv => sv, y => sy); u2 : dev port map (a => sa, b => sb, pv => sv, y => sy); end t;");
    EXPECT_EQ(resolved, kOk) << "std_logic is resolved";
}

TEST(Associations_Actuals, AnInputPortOfTheEnclosingEntityCannotReceiveAnOutput)
{
    const std::string message = analysisError(kEntity + "entity top is port (i : in std_logic); end top; architecture t of top is " + kComponent + kSignals +
                                              " begin u1 : dev port map (a => sa, b => sb, pv => sv, y => i); end t;");
    EXPECT_TRUE(mentions(message, "Port 'i' is an input and cannot be assigned")) << message;
}

// ===========================================================================
// 4. ONE PORT, ELEMENT BY ELEMENT
// ===========================================================================

TEST(Associations_Partial, ElementsOfAnInputVectorCoverItCompletely)
{
    EXPECT_EQ(map("a => sa, b => sb, pv(3) => sa, pv(2) => sb, pv(1) => sa, pv(0) => sb"), kOk);
    EXPECT_EQ(map("a => sa, b => sb, pv(3 downto 2) => sv2, pv(1 downto 0) => sv2"), kOk);
    EXPECT_EQ(map("a => sa, b => sb, pv(3 downto 1) => \"010\", pv(0) => '1'"), kOk);
}

TEST(Associations_Partial, AnInputMustBeCoveredCompletely)
{
    const std::string message = map("a => sa, b => sb, pv(3) => sa, pv(2) => sb");
    EXPECT_TRUE(mentions(message, "Only 2 of the 4 elements of 'pv' are associated")) << message;
}

TEST(Associations_Partial, ElementsCannotOverlapOrLeaveTheVector)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(3) => sa, pv(3) => sb, pv(2 downto 0) => \"000\""), "Element 3 of 'pv' is associated more than once"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(3 downto 1) => \"000\", pv(2 downto 0) => \"000\""), "associated more than once"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(4) => sa"), "outside 'pv'"));
}

TEST(Associations_Partial, OutputsMayBeAssociatedPartially)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, q(3) => sy, q(2 downto 0) => open"), "'open' cannot stand for part of port 'q'"));
    EXPECT_EQ(map("a => sa, b => sb, q(1 downto 0) => sv2"), kOk);
    EXPECT_EQ(map("a => sa, b => sb, q(3) => sy"), kOk);
}

TEST(Associations_Partial, ElementActualsAreTypeChecked)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(3 downto 0) => sn"), "Type mismatch"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(3) => sn, pv(2 downto 0) => \"000\""), "Type mismatch"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(1 downto 0) => sv2, pv(3 downto 2) => sn"), "Type mismatch"));
}

TEST(Associations_Partial, AWholePortAndItsElementsCannotBothBeAssociated)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv => sv, pv(0) => sa"), "associated both as a whole and element by element"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(0) => sa, pv => sv"), "associated both as a whole and element by element"));
}

TEST(Associations_Partial, IndexesMustBeValidForThePort)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, y(0) => sy"), "cannot be indexed or sliced"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, pv(1, 2) => sa"), "needs 1 index value"));
}

// ===========================================================================
// 5. CONVERSIONS
// ===========================================================================

TEST(Associations_Conversions, TypeConversionOnAnOutputFormal)
{
    EXPECT_EQ(map("a => sa, b => sb, std_logic_vector(u) => sq"), kOk);
    EXPECT_EQ(map("a => sa, b => sb, integer(n) => sn"), kOk);
}

TEST(Associations_Conversions, ConversionFunctionOnAnOutputFormal)
{
    const std::string convert = "function to_int(x : unsigned) return integer is begin return 0; end; ";
    EXPECT_EQ(map("a => sa, b => sb, to_int(u) => sn", convert), kOk);
    EXPECT_TRUE(mentions(map("a => sa, b => sb, to_int(u) => sq", convert), "Type mismatch"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, to_int(q) => sn", convert), "No function 'to_int' converts a value of type 'std_logic_vector"));
}

TEST(Associations_Conversions, TheConversionMustExistAndFit)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, nothing(u) => sn"), "must name a type or a function"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, nothing => sn"), "has no port named 'nothing'"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, std_logic(u) => sa"), "not closely related"));
    EXPECT_TRUE(mentions(map("a => sa, b => sb, sa(u) => sn"), "neither a type nor a function"));
}

TEST(Associations_Conversions, InputsConvertOnTheActualSide)
{
    EXPECT_EQ(map("a => sa, b => sb, v => std_logic_vector(su)"), kOk);
    EXPECT_TRUE(mentions(map("a => sa, b => sb, unsigned(v) => su"), "is an input, so a conversion belongs on the actual"));
}

TEST(Associations_Conversions, ConversionResultsAreTypeChecked)
{
    EXPECT_TRUE(mentions(map("a => sa, b => sb, std_logic_vector(u) => sv2"), "Length mismatch"));
}
