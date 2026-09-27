// linker.test.cc — GTest suite for Pulse::Parser::Linker
//
// The linker runs after analysis: every design file has been analyzed into a DesignLibrary, so each architecture has
// already found its entity. The linker binds every component instance to the entity with the same name as its
// component (the default binding of LRM 7.3.3), checks that the component fits that entity, and merges the files into
// one design in which all entities come first and all architectures after them.
//
// Implementation notes:
//   - Every test compiles real VHDL, one string per design file, the way the compiler does: TestUtil::compileFiles()
//     parses each file, analyzes the files in dependency order and links them; TestUtil::compileError() returns the
//     message of the first diagnostic, or "<no error>".
//   - Binding checks a component once, however many instances it has, and only a component that is instantiated.
//   - Generics and ports are matched by name. Types must be the same (lengths are compared when both are known), a port
//     mode must allow the association (LRM 6.5.6.3), and whatever the component leaves out must have a default, except
//     an output or inout port, which stays open.

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "linker.h"
#include "test_helpers.h"

using namespace Pulse::Parser;
using TestUtil::compileError;
using TestUtil::compileFiles;
using TestUtil::parseSource;

namespace
{
    constexpr const char* kOk = "<no error>";

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    template <typename T>
    const T* as(const DesignUnitPtr& unit)
    {
        return dynamic_cast<const T*>(unit.get());
    }

    /// A file with the entity `e`, declared with `generics` and `ports`, and an architecture of it.
    std::string entityFile(const std::string& generics, const std::string& ports)
    {
        return "entity e is " + (generics.empty() ? "" : "generic (" + generics + "); ") + (ports.empty() ? "" : "port (" + ports + "); ")
               + "end e; architecture rtl of e is begin end rtl;";
    }

    /// A file with a top level design that declares the component `component` and instantiates it with `instance`.
    std::string topFile(const std::string& component, const std::string& instance)
    {
        return "entity top is end top; architecture rtl of top is " + component
               + " signal s1, s2 : std_logic; signal w8 : std_logic_vector(7 downto 0); begin " + instance + " end rtl;";
    }

    /// Compiles the file of the entity and the top level file, and returns the first diagnostic: the one of binding the
    /// instance of `e` when both files analyze cleanly.
    std::string linkError(const std::string& entity, const std::string& top)
    {
        return compileError({ entity, top });
    }

    /// The location of the ast_link_error that linking `sources` fails with.
    SourceLocation linkErrorLocation(const std::vector<std::string>& sources)
    {
        try
        {
            compileFiles(sources);
        }
        catch (const ast_link_error& e)
        {
            return e.location();
        }
        ADD_FAILURE() << "expected an ast_link_error";
        return { 0, 0 };
    }
}

// ===========================================================================
// 1. MERGING THE FILES OF A DESIGN
// ===========================================================================

TEST(Linker_Merging, NoUnitsGiveAnEmptyDesign)
{
    EXPECT_TRUE(compileFiles({}).children.empty());
    EXPECT_TRUE(compileFiles({ "", "  -- only a comment\n" }).children.empty());
}

TEST(Linker_Merging, EntitiesComeFirstThenArchitectures)
{
    const ASTRoot design = compileFiles({
        "entity a is end a; architecture rtl of a is begin end rtl; entity b is end b; architecture rtl of b is begin end rtl;",
        "entity c is end c; architecture rtl of c is begin end rtl;",
    });

    ASSERT_EQ(design.children.size(), 6u);
    for (size_t i = 0; i < 3; ++i)
        EXPECT_NE(as<EntityDeclaration>(design.children[i]), nullptr) << "child " << i << " should be an entity";
    for (size_t i = 3; i < 6; ++i)
        EXPECT_NE(as<ArchitectureDeclaration>(design.children[i]), nullptr) << "child " << i << " should be an architecture";

    EXPECT_EQ(as<EntityDeclaration>(design.children[0])->name, "a");
    EXPECT_EQ(as<EntityDeclaration>(design.children[2])->name, "c");
    EXPECT_EQ(as<ArchitectureDeclaration>(design.children[5])->entityName, "c");
}

TEST(Linker_Merging, AnArchitectureJoinsItsEntityFromAnotherFile)
{
    const ASTRoot design = compileFiles({ "architecture rtl of counter is begin q <= clk; end rtl;",
                                          "entity counter is port (clk : in std_logic; q : out std_logic); end counter;" });

    ASSERT_EQ(design.children.size(), 2u);
    EXPECT_EQ(as<EntityDeclaration>(design.children[0])->name, "counter");
    EXPECT_EQ(as<ArchitectureDeclaration>(design.children[1])->entityName, "counter");
}

TEST(Linker_Merging, ContextClausesTravelWithTheirUnit)
{
    const ASTRoot design = compileFiles({ "architecture rtl of e is begin end rtl;",
                                          "library ieee; use ieee.std_logic_1164.all; entity e is end e;" });

    ASSERT_EQ(design.children.size(), 2u);
    EXPECT_EQ(design.children[0]->context.size(), 2u) << "the entity kept its library and use clauses";
    EXPECT_TRUE(design.children[1]->context.empty());
}

TEST(Linker_Merging, ASecondLinkFindsNothingLeft)
{
    DesignLibrary library;
    std::vector<ASTRoot> files = TestUtil::analyzeFiles({ "entity e is end e;" }, library);

    Linker linker(library);
    linker.addAST(std::move(files[0]));

    const ASTRoot first = linker.link();
    const ASTRoot second = linker.link();
    EXPECT_EQ(first.children.size(), 1u);
    EXPECT_TRUE(second.children.empty()) << "the first link took the units";
}

TEST(Linker_Merging, ManyFiles)
{
    std::vector<std::string> files;
    for (int i = 0; i < 500; ++i)
    {
        const std::string name = "e" + std::to_string(i);
        files.push_back("entity " + name + " is end " + name + "; architecture rtl of " + name + " is begin end rtl;");
    }

    ASTRoot design;
    EXPECT_NO_THROW(design = compileFiles(files));
    EXPECT_EQ(design.children.size(), 1000u);
}

// ===========================================================================
// 2. BINDING INSTANCES TO ENTITIES
// ===========================================================================

TEST(Linker_Binding, AnInstanceIsBoundToTheEntityOfItsComponent)
{
    const std::string leaf = "entity leaf is port (i : in std_logic; o : out std_logic); end leaf; architecture rtl of leaf is begin o <= i; end rtl;";
    const std::string top = "entity top is end top; architecture rtl of top is "
                            "component leaf is port (i : in std_logic; o : out std_logic); end component; signal a, b : std_logic; "
                            "begin u1 : leaf port map (i => a, o => b); end rtl;";

    EXPECT_EQ(compileError({ leaf, top }), kOk);
    EXPECT_EQ(compileError({ top, leaf }), kOk) << "the order of the files does not matter";
    EXPECT_EQ(compileError({ leaf + "\n" + top }), kOk) << "both in one file";
}

TEST(Linker_Binding, AnInstanceNeedsAnEntity)
{
    const std::string message = compileError({ "entity top is end top; architecture rtl of top is component ghost is end component; begin u1 : ghost; end rtl;" });
    EXPECT_TRUE(mentions(message, "Component 'ghost' has no entity of the same name, so instance 'u1' cannot be bound")) << message;
}

TEST(Linker_Binding, TheMissingEntityIsReportedAtTheInstance)
{
    const SourceLocation at = linkErrorLocation({ "entity top is end top;\narchitecture rtl of top is\n    component ghost is end component;\n"
                                                  "begin\n    u1 : ghost;\nend rtl;" });
    EXPECT_EQ(at.line, 5u);
    EXPECT_EQ(at.column, 5u) << "an instance is located where its statement starts, at its label";
}

TEST(Linker_Binding, AComponentWithoutInstancesNeedsNoEntity)
{
    EXPECT_EQ(compileError({ "entity top is end top; architecture rtl of top is component ghost is port (a : in std_logic); end component; "
                             "begin end rtl;" }), kOk) << "only an instance is bound (LRM 7.3.3)";
}

TEST(Linker_Binding, EveryArchitectureBindsItsOwnComponent)
{
    // Two architectures declare their own component `e`; only the second one does not fit the entity.
    const std::string good = "architecture a1 of top is component e is port (a : in std_logic); end component; signal s : std_logic; "
                             "begin u1 : e port map (a => s); end a1;";
    const std::string bad = "architecture a2 of top is component e is port (a : in integer); end component; signal n : integer; "
                            "begin u1 : e port map (a => n); end a2;";

    const std::string message = compileError({ entityFile("", "a : in std_logic"), "entity top is end top; " + good + " " + bad });
    EXPECT_TRUE(mentions(message, "Port 'a' of component 'e' has type 'integer' but the entity declares 'std_logic'")) << message;
    EXPECT_EQ(compileError({ entityFile("", "a : in std_logic"), "entity top is end top; " + good }), kOk);
}

TEST(Linker_Binding, AComponentIsCheckedOnceForAllItsInstances)
{
    std::string instances;
    for (int i = 0; i < 2000; ++i)
        instances += "u" + std::to_string(i) + " : e port map (a => s1); ";

    EXPECT_EQ(linkError(entityFile("", "a : in std_logic"), topFile("component e is port (a : in std_logic); end component;", instances)), kOk);
}

// ===========================================================================
// 3. GENERICS
// ===========================================================================

TEST(Linker_Generics, TheComponentMayRestateOrOmitADefault)
{
    const std::string entity = entityFile("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)");
    EXPECT_EQ(linkError(entity, topFile("component e is generic (w : natural := 8); port (d : in std_logic_vector(w - 1 downto 0)); end component;",
                                   "u : e port map (d => w8);")), kOk);
    EXPECT_EQ(linkError(entity, topFile("component e is generic (w : natural); port (d : in std_logic_vector(w - 1 downto 0)); end component;",
                                   "u : e generic map (w => 8) port map (d => w8);")), kOk);
}

TEST(Linker_Generics, EveryGenericOfTheComponentExistsInTheEntity)
{
    const std::string message = linkError(entityFile("w : natural := 8", ""), topFile("component e is generic (other : natural := 1); end component;", "u : e;"));
    EXPECT_TRUE(mentions(message, "Generic 'other' of component 'e' does not exist in entity 'e'")) << message;
}

TEST(Linker_Generics, TheTypesMustBeTheSame)
{
    const std::string message = linkError(entityFile("w : natural := 8", ""), topFile("component e is generic (w : std_logic := '0'); end component;", "u : e;"));
    EXPECT_TRUE(mentions(message, "Generic 'w' of component 'e' has type 'std_logic' but the entity declares 'integer")) << message;
    EXPECT_EQ(linkError(entityFile("w : natural := 8", ""), topFile("component e is generic (w : integer := 8); end component;", "u : e;")), kOk)
        << "natural and integer are one type with different ranges";
}

TEST(Linker_Generics, AnEntityGenericWithoutDefaultMustBeDeclared)
{
    const std::string message = linkError(entityFile("w : natural", ""), topFile("component e is end component;", "u : e;"));
    EXPECT_TRUE(mentions(message, "Component 'e' does not declare the generic 'w' of entity 'e', which has no default value")) << message;
    EXPECT_EQ(linkError(entityFile("w : natural := 4", ""), topFile("component e is end component;", "u : e;")), kOk) << "a default takes its place";
}

// ===========================================================================
// 4. PORTS: NAMES, TYPES AND LENGTHS
// ===========================================================================

TEST(Linker_Ports, EveryPortOfTheComponentExistsInTheEntity)
{
    const std::string message = linkError(entityFile("", "a : in std_logic"),
                                     topFile("component e is port (a : in std_logic; x : in std_logic); end component;", "u : e port map (a => s1, x => s2);"));
    EXPECT_TRUE(mentions(message, "Port 'x' of component 'e' does not exist in entity 'e'")) << message;
}

TEST(Linker_Ports, TheTypesMustBeTheSame)
{
    const std::string message = linkError(entityFile("", "a : in std_logic; b : out std_logic"),
                                     topFile("component e is port (a : in std_logic; b : out std_logic_vector(7 downto 0)); end component;",
                                             "u : e port map (a => s1, b => w8);"));
    EXPECT_TRUE(mentions(message, "Port 'b' of component 'e' has type 'std_logic_vector(7 downto 0)' but the entity declares 'std_logic'")) << message;
}

TEST(Linker_Ports, LengthsKnownOnBothSidesMustAgree)
{
    const std::string entity = entityFile("", "d : in std_logic_vector(7 downto 0)");
    const std::string message = linkError(entity, topFile("component e is port (d : in std_logic_vector(3 downto 0)); end component;", "u : e port map (d => w8(3 downto 0));"));
    EXPECT_TRUE(mentions(message, "has type 'std_logic_vector(3 downto 0)' but the entity declares 'std_logic_vector(7 downto 0)'")) << message;

    EXPECT_EQ(linkError(entity, topFile("component e is port (d : in std_logic_vector(0 to 7)); end component;", "u : e port map (d => w8);")), kOk)
        << "only the length matters, not the bounds or the direction";
    EXPECT_EQ(linkError(entityFile("w : natural := 8", "d : in std_logic_vector(w - 1 downto 0)"),
                   topFile("component e is port (d : in std_logic_vector(7 downto 0)); end component;", "u : e port map (d => w8);")), kOk)
        << "a length that depends on a generic is only known per instance";
}

TEST(Linker_Ports, TheCheckIsReportedAtTheComponentPort)
{
    const SourceLocation at = linkErrorLocation({ entityFile("", "a : in std_logic"),
                                                  "entity top is end top;\narchitecture rtl of top is\n    component e is\n        port (a : in integer);\n"
                                                  "    end component;\n    signal n : integer;\nbegin\n    u : e port map (a => n);\nend rtl;" });
    EXPECT_EQ(at.line, 4u);
    EXPECT_EQ(at.column, 15u);
}

// ===========================================================================
// 5. PORTS: MODES (LRM 6.5.6.3)
// ===========================================================================

TEST(Linker_Modes, AComponentPortBindsToAnEntityPortItCanBeAssociatedWith)
{
    struct Case { const char* entityMode; const char* componentMode; bool binds; };
    const Case cases[] = {
        { "in", "in", true },       { "in", "inout", true },    { "in", "out", false },
        { "out", "out", true },     { "out", "inout", true },   { "out", "in", false },
        { "inout", "inout", true }, { "inout", "in", false },   { "inout", "out", false },
    };

    for (const Case& c : cases)
    {
        const std::string message = linkError(entityFile("", std::string("p : ") + c.entityMode + " std_logic"),
                                         topFile(std::string("component e is port (p : ") + c.componentMode + " std_logic); end component;",
                                                 "u : e port map (p => s1);"));
        const std::string what = std::string("entity ") + c.entityMode + ", component " + c.componentMode + ": " + message;

        if (c.binds)
            EXPECT_EQ(message, kOk) << what;
        else
            EXPECT_TRUE(mentions(message, std::string("Port 'p' of component 'e' has mode '") + c.componentMode + "' but the entity declares '"
                                          + c.entityMode + "'")) << what;
    }
}

TEST(Linker_Modes, TheMessageSaysWhichModesFit)
{
    const std::string message = linkError(entityFile("", "p : out std_logic"), topFile("component e is port (p : in std_logic); end component;", "u : e port map (p => s1);"));
    EXPECT_TRUE(mentions(message, "which only a port of mode 'out' or 'inout' can be bound to")) << message;
}

// ===========================================================================
// 6. WHAT THE COMPONENT LEAVES OUT
// ===========================================================================

TEST(Linker_Omissions, AnInputWithoutDefaultMustBeDeclared)
{
    const std::string component = "component e is port (a : in std_logic); end component;";
    const std::string message = linkError(entityFile("", "a : in std_logic; b : in std_logic"), topFile(component, "u : e port map (a => s1);"));
    EXPECT_TRUE(mentions(message, "Component 'e' does not declare the input port 'b' of entity 'e', which has no default value")) << message;

    EXPECT_EQ(linkError(entityFile("", "a : in std_logic; b : in std_logic := '0'"), topFile(component, "u : e port map (a => s1);")), kOk)
        << "the default gives the input its value";
}

TEST(Linker_Omissions, OutputsAndInoutsMayStayOpen)
{
    EXPECT_EQ(linkError(entityFile("", "a : in std_logic; y : out std_logic; io : inout std_logic"),
                   topFile("component e is port (a : in std_logic); end component;", "u : e port map (a => s1);")), kOk);
}

// ===========================================================================
// 7. ERRORS AND TREES THAT WERE NOT ANALYZED
// ===========================================================================

TEST(Linker_Errors, LinkErrorsAreCompilerErrors)
{
    const std::vector<std::string> unbound = { "entity top is end top; architecture rtl of top is component ghost is end component; begin u1 : ghost; end rtl;" };
    EXPECT_THROW(compileFiles(unbound), ast_link_error);
    EXPECT_THROW(compileFiles(unbound), compiler_error);
}

TEST(Linker_Errors, AFailedLinkLeavesTheFilesInTheLinker)
{
    DesignLibrary library;
    std::vector<ASTRoot> files = TestUtil::analyzeFiles(
        { "entity top is end top; architecture rtl of top is component ghost is end component; begin u1 : ghost; end rtl;" }, library);

    Linker linker(library);
    linker.addAST(std::move(files[0]));
    EXPECT_THROW(linker.link(), ast_link_error);
    EXPECT_THROW(linker.link(), ast_link_error) << "binding comes before merging, so nothing was taken";
}

TEST(Linker_Errors, AnInstanceThatWasNotAnalyzedIsRefused)
{
    DesignLibrary library;
    Linker linker(library);
    linker.addAST(parseSource("entity leaf is end leaf; entity top is end top; "
                              "architecture rtl of top is component leaf is end component; begin u1 : leaf; end rtl;"));

    try
    {
        linker.link();
        FAIL() << "expected ast_link_error";
    }
    catch (const ast_link_error& e)
    {
        EXPECT_TRUE(mentions(e.what(), "Instance 'u1' was not analyzed")) << e.what();
    }
}

TEST(Linker_Errors, AnEntityThatWasNotAnalyzedIsRefused)
{
    // The top level file is analyzed, and its component with it; the file of the entity is only parsed.
    DesignLibrary library;
    ASTRoot leaf = parseSource("entity leaf is port (a : in std_logic); end leaf;");
    ASTRoot top = parseSource("entity top is end top; architecture rtl of top is component leaf is port (a : in std_logic); end component; "
                              "signal s : std_logic; begin u1 : leaf port map (a => s); end rtl;");
    library.analyze(top);

    Linker linker(library);
    linker.addAST(std::move(leaf));
    linker.addAST(std::move(top));

    try
    {
        linker.link();
        FAIL() << "expected ast_link_error";
    }
    catch (const ast_link_error& e)
    {
        EXPECT_TRUE(mentions(e.what(), "The port 'a' of entity 'leaf' was not analyzed")) << e.what();
    }
}

namespace
{
    /// A design unit the parser cannot build yet (a package, one day): the linker passes it through.
    struct OpaqueUnit final : DesignUnit
    {
        std::unique_ptr<ASTNode> clone() const override { return std::make_unique<OpaqueUnit>(); }
        void print(int) const override { }
    };
}

TEST(Linker_Trees, OtherUnitsAreKeptAheadOfEntities)
{
    DesignLibrary library;
    std::vector<ASTRoot> files = TestUtil::analyzeFiles({ "entity e is end e; architecture rtl of e is begin end rtl;" }, library);

    ASTRoot other;
    other.children.push_back(std::make_unique<OpaqueUnit>());

    Linker linker(library);
    linker.addAST(std::move(files[0]));
    linker.addAST(std::move(other));
    const ASTRoot design = linker.link();

    ASSERT_EQ(design.children.size(), 3u);
    EXPECT_NE(as<OpaqueUnit>(design.children[0]), nullptr);
    EXPECT_NE(as<EntityDeclaration>(design.children[1]), nullptr);
    EXPECT_NE(as<ArchitectureDeclaration>(design.children[2]), nullptr);
}

TEST(Linker_Trees, NullUnitsAreSkipped)
{
    DesignLibrary library;
    ASTRoot root = parseSource("entity e is end e; architecture rtl of e is begin end rtl;");
    root.children.insert(root.children.begin(), nullptr);
    root.children.push_back(nullptr);
    library.analyze(root);

    Linker linker(library);
    linker.addAST(std::move(root));

    ASTRoot design;
    EXPECT_NO_THROW(design = linker.link());
    EXPECT_EQ(design.children.size(), 2u);
}
