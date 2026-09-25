// analyzer_generics.test.cc — generics of entities and components, generic maps, port defaults, and types whose bounds depend
// on a generic (they are constants of an instance, so the checks that need their value are skipped).

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

    /// A design: the entity `e` with generics `generics`, ports `ports`, and an architecture with `declarations` and `body`.
    std::string design(const std::string& generics, const std::string& ports, const std::string& declarations = "", const std::string& body = "")
    {
        return "entity e is " + (generics.empty() ? "" : "generic (" + generics + "); ") + (ports.empty() ? "" : "port (" + ports + "); ") + "end e; "
               "architecture r of e is " + declarations + " begin " + body + " end r;";
    }

    std::string check(const std::string& generics, const std::string& ports, const std::string& declarations = "", const std::string& body = "")
    {
        return analysisError(design(generics, ports, declarations, body));
    }

    /// A top level design that instantiates the entity `e` (declared with `entityGenerics` and `entityPorts`).
    std::string top(const std::string& entityGenerics, const std::string& entityPorts, const std::string& component, const std::string& instance,
                    const std::string& topDeclarations = "signal s1, s2 : std_logic; signal w8 : std_logic_vector(7 downto 0); signal n : integer; ")
    {
        return analysisError("entity e is " + (entityGenerics.empty() ? "" : "generic (" + entityGenerics + "); ")
                             + (entityPorts.empty() ? "" : "port (" + entityPorts + "); ") + "end e; architecture r of e is begin end r; "
                             "entity top is end top; architecture t of top is " + component + topDeclarations + " begin " + instance + " end t;");
    }
}

// ===========================================================================
// 1. GENERIC DECLARATIONS
// ===========================================================================

TEST(Generics_Declarations, GenericsAreConstantsOfTheArchitecture)
{
    EXPECT_EQ(check("w : natural := 8", "a : in std_logic", "constant k : integer := w * 2; ", "process begin wait for w * 1 ns; end process;"), kOk);
}

TEST(Generics_Declarations, DefaultsAreOptional)
{
    EXPECT_EQ(check("w : natural; n : integer := 4", "a : in std_logic"), kOk);
}

TEST(Generics_Declarations, DefaultsAreTypeChecked)
{
    EXPECT_TRUE(mentions(check("w : natural := '1'", ""), "Type mismatch"));
    EXPECT_TRUE(mentions(check("w : natural := -1", ""), "outside the range"));
    EXPECT_TRUE(mentions(check("w : nothing := 1", ""), "Unknown type 'nothing'"));
}

TEST(Generics_Declarations, DefaultsCanUseEarlierGenerics)
{
    EXPECT_EQ(check("a : integer := 1; b : integer := a + 1", ""), kOk);
    EXPECT_TRUE(mentions(check("b : integer := a + 1; a : integer := 1", ""), "'a' is not declared"));
}

TEST(Generics_Declarations, GenericsCannotBeAssigned)
{
    const std::string message = check("w : integer := 1", "", "signal s : integer; ", "process begin w <= 1; wait; end process;");
    EXPECT_TRUE(mentions(message, "'w' is a constant and cannot be assigned")) << message;
}

TEST(Generics_Declarations, NamesMustBeUnique)
{
    EXPECT_TRUE(mentions(check("w : integer; w : integer", ""), "already declared"));
    EXPECT_TRUE(mentions(check("w : integer", "w : in std_logic"), "already declared"));
    EXPECT_TRUE(mentions(check("w : integer", "", "signal w : integer; "), "already declared"));
}

TEST(Generics_Declarations, GroupedNamesDeclareOneGenericEach)
{
    EXPECT_EQ(check("a, b, c : integer := 3", "", "constant k : integer := a + b + c; "), kOk);
}

// ===========================================================================
// 2. BOUNDS THAT DEPEND ON A GENERIC
// ===========================================================================

TEST(Generics_Bounds, PortsAndSignalsMayUseAGeneric)
{
    EXPECT_EQ(check("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0); q : out std_logic_vector(w - 1 downto 0)",
                    "signal inner : std_logic_vector(w - 1 downto 0); ", "inner <= d; q <= inner;"), kOk);
}

TEST(Generics_Bounds, LengthsAreNotComparedWhenOneIsUnknown)
{
    EXPECT_EQ(check("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)", "signal fixed : std_logic_vector(3 downto 0); ",
                    "fixed <= d;"), kOk);
    EXPECT_EQ(check("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)", "signal fixed : std_logic_vector(3 downto 0); ",
                    "process begin if d = fixed then null; end if; wait; end process;"), kOk);
}

TEST(Generics_Bounds, TypesAreStillChecked)
{
    const std::string message = check("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)", "signal x : std_logic; ", "x <= d;");
    EXPECT_TRUE(mentions(message, "Type mismatch")) << message;
    EXPECT_TRUE(mentions(check("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)", "signal u : unsigned(3 downto 0); ", "u <= d;"),
                         "Type mismatch"));
}

TEST(Generics_Bounds, AggregatesAndStringsAdaptToAnUnknownLength)
{
    EXPECT_EQ(check("w : natural := 8", "q : out std_logic_vector(w - 1 downto 0)", "", "q <= (others => '0');"), kOk);
    EXPECT_EQ(check("w : natural := 8", "q : out std_logic_vector(w - 1 downto 0)", "", "q <= (0 => '1', others => '0');"), kOk);
    EXPECT_EQ(check("w : natural := 8", "q : out std_logic_vector(w - 1 downto 0)", "", "q <= \"0101\";"), kOk);
    EXPECT_TRUE(mentions(check("w : natural := 8", "q : out std_logic_vector(w - 1 downto 0)", "", "q <= (0 => '1', 0 => '0', others => '1');"),
                         "defined twice"));
}

TEST(Generics_Bounds, IndexesSlicesAndAttributes)
{
    const std::string ports = "d : in std_logic_vector(w - 1 downto 0)";
    EXPECT_EQ(check("w : natural := 8", ports, "signal x : std_logic; signal y : std_logic_vector(1 downto 0); signal n, n2 : integer; ",
                    "x <= d(0); y <= d(1 downto 0); n <= d'length; n2 <= d'high;"), kOk);
    EXPECT_EQ(check("w : natural := 8", ports, "signal n : integer; ", "process begin for i in d'range loop n <= i; end loop; wait; end process;"), kOk);
    EXPECT_EQ(check("w : natural := 8", ports, "signal copy : std_logic_vector(d'range); "), kOk) << "a range attribute of an unknown range is constant too";
}

TEST(Generics_Bounds, ArithmeticOnVectorsOfUnknownWidth)
{
    EXPECT_EQ(check("w : natural := 8", "a, b : in unsigned(w - 1 downto 0); s : out unsigned(w - 1 downto 0)", "", "s <= a + b; s <= a + 1;"), kOk);
    EXPECT_EQ(check("w : natural := 8", "a : in unsigned(w - 1 downto 0)", "signal x : unsigned(3 downto 0); ", "x <= a + x;"), kOk);
}

TEST(Generics_Bounds, ScalarSubtypesWithGenericRanges)
{
    EXPECT_EQ(check("w : natural := 8", "", "subtype small is integer range 0 to w; signal s : small; ", "s <= 3;"), kOk);
    EXPECT_EQ(check("w : natural := 8", "", "signal r : integer range 0 to w; ", "process begin for i in 0 to w loop r <= i; end loop; wait; end process;"), kOk);
}

TEST(Generics_Bounds, ArrayTypesWithGenericBounds)
{
    EXPECT_EQ(check("w : natural := 4", "", "type mem is array (0 to w - 1) of std_logic; signal m : mem; ", "m <= (others => '0');"), kOk);
    EXPECT_EQ(check("w : natural := 4", "", "type mem is array (0 to w - 1) of std_logic; signal m : mem; signal x : std_logic; ", "x <= m(0);"), kOk);
}

TEST(Generics_Bounds, ConstantsOfAnUnknownValueAreStillConstants)
{
    EXPECT_EQ(check("w : natural := 8", "", "constant k : integer := w * 2; signal s : std_logic_vector(k downto 0); "), kOk);
    EXPECT_TRUE(mentions(check("w : natural := 8", "", "signal n : integer; signal s : std_logic_vector(n downto 0); "), "constant at analysis time"))
        << "a signal is never a bound";
}

TEST(Generics_Bounds, ConstantsWithAnUnconstrainedTypeTakeTheUnknownBoundsOfTheirValue)
{
    EXPECT_EQ(check("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)", "", "process begin wait; end process;"), kOk);
}

TEST(Generics_Bounds, CaseOverAVectorOfUnknownLengthNeedsOthers)
{
    const std::string ports = "d : in std_logic_vector(w - 1 downto 0)";
    EXPECT_EQ(check("w : natural := 2", ports, "", "process (d) begin case d is when \"00\" => null; when others => null; end case; end process;"), kOk);
    EXPECT_TRUE(mentions(check("w : natural := 2", ports, "", "process (d) begin case d is when \"00\" => null; end case; end process;"), "others"));
}

// ===========================================================================
// 3. COMPONENTS AND ENTITIES
// ===========================================================================

TEST(Generics_Components, TheComponentMustMatchTheGenericsOfItsEntity)
{
    const std::string entityGenerics = "w : natural := 8";
    const std::string entityPorts = "d : in std_logic_vector(w - 1 downto 0)";
    EXPECT_EQ(top(entityGenerics, entityPorts, "component e is generic (w : natural := 8); port (d : in std_logic_vector(w - 1 downto 0)); end component; ",
                  "u : e port map (d => w8);"), kOk);
    EXPECT_EQ(top(entityGenerics, entityPorts, "component e is generic (w : natural); port (d : in std_logic_vector(w - 1 downto 0)); end component; ",
                  "u : e generic map (w => 8) port map (d => w8);"), kOk)
        << "the component may omit the default";
}

TEST(Generics_Components, GenericMismatchesAreReported)
{
    const std::string entity = "w : natural := 8";
    EXPECT_TRUE(mentions(top(entity, "", "component e is generic (other : natural); end component; ", ""),
                         "Generic 'other' of component 'e' does not exist in entity 'e'"));
    EXPECT_TRUE(mentions(top(entity, "", "component e is generic (w : std_logic); end component; ", ""), "has type 'std_logic' but the entity declares"));
}

TEST(Generics_Components, ComponentGenericsAreVisibleInTheirPorts)
{
    EXPECT_EQ(top("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)",
                  "component e is generic (w : natural := 8); port (d : in std_logic_vector(w - 1 downto 0)); end component; ", ""), kOk);
    EXPECT_TRUE(mentions(top("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)",
                             "component e is port (d : in std_logic_vector(w - 1 downto 0)); end component; ", ""), "'w' is not declared"));
}

TEST(Generics_Components, LengthsThatAreKnownOnBothSidesMustAgree)
{
    EXPECT_TRUE(mentions(top("", "d : in std_logic_vector(7 downto 0)", "component e is port (d : in std_logic_vector(3 downto 0)); end component; ", ""),
                         "has type"));
    EXPECT_EQ(top("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)", "component e is port (d : in std_logic_vector(7 downto 0)); end component; ",
                  ""), kOk) << "an unknown length is compatible with a known one";
}

// ===========================================================================
// 4. GENERIC MAPS
// ===========================================================================

namespace
{
    const std::string kComponent = "component e is generic (w : natural := 8; k : integer := 1); port (d : in std_logic); end component; ";
    const std::string kEntityGenerics = "w : natural := 8; k : integer := 1";

    std::string instance(const std::string& genericMap)
    {
        return top(kEntityGenerics, "d : in std_logic", kComponent, "u : e " + genericMap + " port map (d => s1);");
    }
}

TEST(Generics_Maps, NamedPositionalAndMissingMapsAreAllowed)
{
    EXPECT_EQ(instance(""), kOk) << "every generic has a default";
    EXPECT_EQ(instance("generic map (w => 4)"), kOk);
    EXPECT_EQ(instance("generic map (4, 2)"), kOk);
    EXPECT_EQ(instance("generic map (k => 2, w => 4)"), kOk);
    EXPECT_EQ(instance("generic map (4, k => 2)"), kOk);
}

TEST(Generics_Maps, OpenUsesTheDefault)
{
    EXPECT_EQ(instance("generic map (w => open)"), kOk);
}

TEST(Generics_Maps, ActualsAreTypeChecked)
{
    EXPECT_TRUE(mentions(instance("generic map (w => '1')"), "Type mismatch"));
    EXPECT_TRUE(mentions(instance("generic map (w => -3)"), "outside the range"));
}

TEST(Generics_Maps, ActualsMustBeConstant)
{
    const std::string message = instance("generic map (w => n)");
    EXPECT_TRUE(mentions(message, "must be constant")) << message;
}

TEST(Generics_Maps, ActualsCanBeConstantsOfTheEnclosingDesign)
{
    EXPECT_EQ(top(kEntityGenerics, "d : in std_logic", kComponent, "u : e generic map (w => size) port map (d => s1);",
                  "signal s1 : std_logic; constant size : integer := 4; "), kOk);
}

TEST(Generics_Maps, AssociationErrors)
{
    EXPECT_TRUE(mentions(instance("generic map (other => 1)"), "has no generic named 'other'"));
    EXPECT_TRUE(mentions(instance("generic map (w => 1, w => 2)"), "connected twice"));
    EXPECT_TRUE(mentions(instance("generic map (1, 2, 3)"), "Too many associations"));
    EXPECT_TRUE(mentions(instance("generic map (w => 1, 2)"), "positional association cannot follow"));
}

TEST(Generics_Maps, GenericsWithoutADefaultMustBeAssociated)
{
    const std::string component = "component e is generic (w : natural); port (d : in std_logic); end component; ";
    EXPECT_EQ(top("w : natural", "d : in std_logic", component, "u : e generic map (w => 2) port map (d => s1);"), kOk);
    const std::string message = top("w : natural", "d : in std_logic", component, "u : e port map (d => s1);");
    EXPECT_TRUE(mentions(message, "Generic 'w' of instance 'u' is not associated and has no default value")) << message;
    EXPECT_TRUE(mentions(top("w : natural", "d : in std_logic", component, "u : e generic map (w => open) port map (d => s1);"), "cannot be left open"));
}

TEST(Generics_Maps, ConversionsBelongToTheActualNotToTheFormal)
{
    EXPECT_TRUE(mentions(instance("generic map (integer(w) => 4)"), "is an input, so a conversion belongs on the actual"));
}

TEST(Generics_Maps, ElementsOfAnArrayGenericAreAssociatedOneByOne)
{
    const std::string component = "component e is generic (init : std_logic_vector(1 downto 0)); port (d : in std_logic); end component; ";
    const std::string entityGenerics = "init : std_logic_vector(1 downto 0)";
    EXPECT_EQ(top(entityGenerics, "d : in std_logic", component, "u : e generic map (init(1) => '1', init(0) => '0') port map (d => s1);"), kOk);
    EXPECT_TRUE(mentions(top(entityGenerics, "d : in std_logic", component, "u : e generic map (init(1) => '1') port map (d => s1);"), "Only 1 of the 2"));
}

// ===========================================================================
// 5. PORT DEFAULTS
// ===========================================================================

TEST(Generics_PortDefaults, AnInputWithADefaultMayBeLeftUnconnected)
{
    const std::string component = "component e is port (d : in std_logic; en : in std_logic := '1'); end component; ";
    EXPECT_EQ(top("", "d : in std_logic; en : in std_logic := '1'", component, "u : e port map (d => s1);"), kOk);
    EXPECT_EQ(top("", "d : in std_logic; en : in std_logic := '1'", component, "u : e port map (d => s1, en => open);"), kOk);
    EXPECT_EQ(top("", "d : in std_logic; en : in std_logic := '1'", component, "u : e port map (d => s1, en => s2);"), kOk);
}

TEST(Generics_PortDefaults, AnInputWithoutADefaultMustBeConnected)
{
    const std::string component = "component e is port (d : in std_logic; en : in std_logic); end component; ";
    EXPECT_TRUE(mentions(top("", "d : in std_logic; en : in std_logic", component, "u : e port map (d => s1);"), "Input port 'en' of instance 'u' is not connected"));
    EXPECT_TRUE(mentions(top("", "d : in std_logic; en : in std_logic", component, "u : e port map (d => s1, en => open);"), "cannot be left open"));
}

TEST(Generics_PortDefaults, DefaultsAreCheckedAgainstThePortType)
{
    EXPECT_TRUE(mentions(check("", "d : in std_logic := 5"), "Type mismatch"));
    EXPECT_TRUE(mentions(check("", "d : in std_logic := x\"0\""), "cannot be used where 'std_logic' is expected"));
    EXPECT_EQ(check("", "d : in std_logic_vector(3 downto 0) := \"0000\""), kOk);
    EXPECT_EQ(check("", "d : in std_logic_vector(3 downto 0) := (others => '0')"), kOk);
    EXPECT_TRUE(mentions(check("", "d : in std_logic_vector(3 downto 0) := \"00\""), "element"));
}

TEST(Generics_PortDefaults, DefaultsCanUseGenerics)
{
    EXPECT_EQ(check("init : std_logic := '1'", "d : in std_logic := init"), kOk);
}

TEST(Generics_PortDefaults, ComponentPortsWithDefaultsMatchTheirEntity)
{
    EXPECT_EQ(top("", "d : in std_logic := '0'", "component e is port (d : in std_logic := '0'); end component; ", "u : e port map (d => s1);"), kOk);
}
