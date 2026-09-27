// library.test.cc — the design library: design files are analyzed into it one at a time, the design units of each in
// textual order, so an architecture finds its entity even when the entity lives in another file. analysisOrder() gives
// the order in which a set of files is analyzed; the library answers what the linker asks about the analyzed units.

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test_helpers.h"

using namespace Pulse::Parser;
using TestUtil::parseSource;

namespace
{
    constexpr const char* kOk = "<no error>";

    bool mentions(const std::string& message, const std::string& text)
    {
        return message.find(text) != std::string::npos;
    }

    /// Analyzes every source as a design file of its own, in dependency order, and returns the first diagnostic.
    std::string analysisError(const std::vector<std::string>& sources)
    {
        try
        {
            DesignLibrary library;
            TestUtil::analyzeFiles(sources, library);
        }
        catch (const compiler_error& e)
        {
            return e.what();
        }
        return "<no error>";
    }

    /// The order in which the files of the given sources are analyzed.
    std::vector<size_t> orderOf(const std::vector<std::string>& sources)
    {
        std::vector<ASTRoot> files;
        for (const std::string& source : sources)
            files.push_back(parseSource(source));
        return analysisOrder(files);
    }

    const std::string kCounter =
        "entity counter is generic (w : natural := 4); port (clk : in std_logic; q : out unsigned(w - 1 downto 0)); end counter;";
}

// ===========================================================================
// 1. ENTITIES AND ARCHITECTURES IN DIFFERENT FILES
// ===========================================================================

TEST(Library_Files, AnArchitectureCanLiveInAnotherFileThanItsEntity)
{
    const std::string architecture = "architecture rtl of counter is signal n : unsigned(w - 1 downto 0); begin "
                                     "process (clk) begin if clk = '1' then n <= n + 1; end if; end process; q <= n; end rtl;";
    EXPECT_EQ(analysisError({ kCounter, architecture }), kOk);
    EXPECT_EQ(analysisError({ architecture, kCounter }), kOk) << "the file of the entity is analyzed first";
}

TEST(Library_Files, TheEntityOfAnotherFileIsCheckedLikeOneOfTheSameFile)
{
    EXPECT_TRUE(mentions(analysisError({ kCounter, "architecture rtl of counter is begin clk <= '1'; end rtl;" }),
                         "Port 'clk' is an input and cannot be assigned"));
    EXPECT_TRUE(mentions(analysisError({ kCounter, "architecture rtl of counter is begin q <= clk; end rtl;" }), "Type mismatch"));
    EXPECT_TRUE(mentions(analysisError({ kCounter, "architecture rtl of counter is signal clk : std_logic; begin end rtl;" }), "already declared"))
        << "the ports of the entity are declared in the region of the architecture";
    const std::string generic = analysisError({ kCounter, "architecture rtl of counter is begin assert w = '1'; end rtl;" });
    EXPECT_TRUE(mentions(generic, "Operator '=' cannot be applied to 'integer range 0 to 2147483647' and 'std_logic'"))
        << "the generics of the entity are visible with their type: " << generic;
}

TEST(Library_Files, WhatAnArchitectureDeclaresStaysInIt)
{
    const std::string first = "entity a is end a; architecture rtl of a is type state is (idle, run); signal s : state; begin end rtl;";
    const std::string second = "entity b is end b; architecture rtl of b is signal s : state; begin end rtl;";
    EXPECT_TRUE(mentions(analysisError({ first, second }), "Unknown type 'state'"));
}

TEST(Library_Files, ErrorsAreLocatedInTheFileTheyAreIn)
{
    try
    {
        DesignLibrary library;
        TestUtil::analyzeFiles({ "entity e is port (a : in std_logic); end e;", "architecture rtl of e is\nbegin\n    a <= '1';\nend rtl;" }, library);
        FAIL() << "expected an ast_semantic_error";
    }
    catch (const ast_semantic_error& e)
    {
        EXPECT_EQ(e.location().line, 3u);
        EXPECT_EQ(e.location().column, 5u);
    }
}

// ===========================================================================
// 2. THE ORDER OF ANALYSIS
// ===========================================================================

TEST(Library_Order, AFileComesAfterTheFilesOfItsEntities)
{
    EXPECT_EQ(orderOf({ "architecture rtl of e is begin end rtl;", "entity e is end e;" }), (std::vector<size_t>{ 1, 0 }));
}

TEST(Library_Order, IndependentFilesKeepTheirOrder)
{
    EXPECT_EQ(orderOf({ "entity a is end a;", "entity b is end b; architecture rtl of b is begin end rtl;", "entity c is end c;" }),
              (std::vector<size_t>{ 0, 1, 2 }));
    EXPECT_TRUE(orderOf({}).empty());
}

TEST(Library_Order, ChainsAndSharedDependencies)
{
    // The first file needs `b`, which the second declares; the second needs `c`, which the third declares.
    EXPECT_EQ(orderOf({ "architecture rtl of b is begin end rtl;", "entity b is end b; architecture rtl of c is begin end rtl;", "entity c is end c;" }),
              (std::vector<size_t>{ 2, 1, 0 }));

    // The first file needs `a` and `b`; the last file needs `a` too, which is placed once.
    EXPECT_EQ(orderOf({ "architecture rtl of a is begin end rtl; architecture rtl of b is begin end rtl;", "entity a is end a;", "entity b is end b;",
                        "architecture other of a is begin end other;" }),
              (std::vector<size_t>{ 1, 2, 0, 3 }));
}

TEST(Library_Order, ALongChainOfFiles)
{
    // Every file needs the entity of the next one, so the last file is analyzed first.
    const size_t count = 5000;
    std::vector<std::string> sources;
    for (size_t i = 0; i < count; ++i)
    {
        const std::string name = "e" + std::to_string(i);
        const std::string next = "e" + std::to_string(i + 1);
        sources.push_back("entity " + name + " is end " + name + ";" + (i + 1 < count ? " architecture rtl of " + next + " is begin end rtl;" : ""));
    }

    const std::vector<size_t> order = orderOf(sources);
    ASSERT_EQ(order.size(), count);
    EXPECT_EQ(order.front(), count - 1);
    EXPECT_EQ(order.back(), 0u);
    EXPECT_EQ(analysisError(sources), kOk);
}

TEST(Library_Order, FilesThatNeedEachOtherAreRefused)
{
    const std::vector<std::string> sources = { "entity a is end a; architecture rtl of b is begin end rtl;",
                                               "entity b is end b; architecture rtl of a is begin end rtl;" };
    const std::string message = analysisError(sources);
    EXPECT_TRUE(mentions(message, "needs the entity 'a' of another file, which itself depends on this file, so neither file can be analyzed first"))
        << message;
    EXPECT_THROW(orderOf(sources), ast_semantic_error);
}

TEST(Library_Order, ACycleThroughSeveralFilesIsRefused)
{
    const std::string message = analysisError({ "entity a is end a; architecture rtl of b is begin end rtl;",
                                                "entity b is end b; architecture rtl of c is begin end rtl;",
                                                "entity c is end c; architecture rtl of a is begin end rtl;" });
    EXPECT_TRUE(mentions(message, "so neither file can be analyzed first")) << message;
}

TEST(Library_Order, AnArchitectureAboveItsEntityIsRefused)
{
    const std::string message = analysisError({ "architecture rtl of e is begin end rtl; entity e is end e;" });
    EXPECT_TRUE(mentions(message, "Architecture 'rtl' comes before its entity 'e'; an entity must be analyzed before its architectures")) << message;
}

TEST(Library_Order, AnArchitectureOfAnEntityNoFileDeclares)
{
    const std::string message = analysisError({ "entity a is end a;", "architecture rtl of b is begin end rtl;" });
    EXPECT_TRUE(mentions(message, "Architecture 'rtl' belongs to the unknown entity 'b'")) << message;
}

// ===========================================================================
// 3. NAMES IN THE LIBRARY
// ===========================================================================

TEST(Library_Names, AnEntityNameIsUniqueInTheLibrary)
{
    EXPECT_TRUE(mentions(analysisError({ "entity e is end e;", "entity e is end e;" }), "Entity 'e' is declared twice"));
    EXPECT_TRUE(mentions(analysisError({ "entity Adder is end Adder;", "ENTITY ADDER IS END;" }), "Entity 'adder' is declared twice"))
        << "VHDL names are not case-sensitive";
}

TEST(Library_Names, AnArchitectureNameIsUniquePerEntity)
{
    EXPECT_TRUE(mentions(analysisError({ "entity e is end e; architecture rtl of e is begin end rtl;", "architecture RTL of e is begin end RTL;" }),
                         "Architecture 'rtl' of entity 'e' is declared twice"));
    EXPECT_EQ(analysisError({ "entity e is end e; architecture rtl of e is begin end rtl; architecture behavioral of e is begin end behavioral;" }), kOk)
        << "an entity may have several architectures";
    EXPECT_EQ(analysisError({ "entity a is end a; architecture rtl of a is begin end rtl;", "entity b is end b; architecture rtl of b is begin end rtl;" }), kOk)
        << "two entities may each have an architecture of the same name";
    EXPECT_EQ(analysisError({ "entity e is end e; architecture e of e is begin end e;" }), kOk) << "an architecture may be named like its entity";
}

// ===========================================================================
// 4. WHAT THE LINKER ASKS
// ===========================================================================

TEST(Library_Queries, TheComponentAnInstanceInstantiates)
{
    const std::string source = "entity top is end top; architecture rtl of top is component leaf is port (i : in std_logic); end component; "
                               "signal s : std_logic; begin u1 : leaf port map (i => s); end rtl;";
    const ASTRoot file = parseSource(source);
    DesignLibrary library;
    library.analyze(file);

    const ArchitectureDeclaration* architecture = TestUtil::firstArchitecture(file);
    const ComponentDeclaration* component = TestUtil::declaredAs<ComponentDeclaration>(architecture->declarations).at(0);
    const auto* instance = TestUtil::nodeAs<ComponentInstantiation>(architecture->body.at(0).get());
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(library.componentOf(*instance), component);

    const ASTRoot other = parseSource(source);
    EXPECT_EQ(library.componentOf(*TestUtil::nodeAs<ComponentInstantiation>(TestUtil::firstArchitecture(other)->body.at(0).get())), nullptr)
        << "a tree that was not analyzed into the library";
}

TEST(Library_Queries, TheTypesOfGenericsAndPorts)
{
    const ASTRoot file = parseSource("entity e is generic (w : natural := 4); port (d : in std_logic_vector(7 downto 0); q : out unsigned(w - 1 downto 0)); end e; "
                                     "entity top is end top; architecture rtl of top is component c is port (a : in integer); end component; begin end rtl;");
    DesignLibrary library;
    library.analyze(file);

    const auto* entity = TestUtil::nodeAs<EntityDeclaration>(file.children.at(0).get());
    const SemanticType* generic = library.interfaceType(*entity->generics.at(0));
    const SemanticType* d = library.interfaceType(*entity->ports.at(0));
    const SemanticType* q = library.interfaceType(*entity->ports.at(1));
    ASSERT_TRUE(generic && d && q);
    EXPECT_EQ(describe(*generic), "integer range 0 to 2147483647");
    EXPECT_EQ(describe(*d), "std_logic_vector(7 downto 0)");
    EXPECT_EQ(describe(*q), "unsigned(...)") << "its bounds depend on a generic";

    const ComponentDeclaration* component = TestUtil::declaredAs<ComponentDeclaration>(TestUtil::firstArchitecture(file)->declarations).at(0);
    const SemanticType* a = library.interfaceType(*component->ports.at(0));
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(describe(*a), "integer");

    const ASTRoot other = parseSource("entity f is port (a : in std_logic); end f;");
    EXPECT_EQ(library.interfaceType(*TestUtil::nodeAs<EntityDeclaration>(other.children.at(0).get())->ports.at(0)), nullptr);
}

// ===========================================================================
// 5. A LIBRARY OUTLIVES A FAILED FILE
// ===========================================================================

TEST(Library_Robustness, AFailedFileLeavesTheLibraryUsable)
{
    const ASTRoot first = parseSource("entity e is port (a : in std_logic); end e;");
    const ASTRoot failing = parseSource("entity f is end f; architecture rtl of f is signal s : std_logic; signal t : nothing; begin end rtl;");
    const ASTRoot later = parseSource("architecture rtl of e is signal s : std_logic; begin s <= a; end rtl; architecture rtl of f is begin end rtl;");

    DesignLibrary library;
    library.analyze(first);
    EXPECT_THROW(library.analyze(failing), ast_semantic_error);
    EXPECT_NO_THROW(library.analyze(later)) << "the failed architecture left no scope behind and never joined the library, but its entity did";
}

// ===========================================================================
// 6. WHAT THE ELABORATOR READS
// ===========================================================================

TEST(Library_Elaboration, ArchitecturesAreFoundInAnalysisOrder)
{
    const ASTRoot file = parseSource("entity e is end e; architecture a1 of e is begin end a1; architecture a2 of e is begin end a2;");
    DesignLibrary library;
    library.analyze(file);

    const auto* entity = TestUtil::nodeAs<EntityDeclaration>(file.children.at(0).get());
    EXPECT_EQ(library.entity("e"), entity);
    EXPECT_EQ(library.entity("missing"), nullptr);

    ASSERT_NE(library.latestArchitecture("e"), nullptr);
    EXPECT_EQ(library.latestArchitecture("e")->name, "a2") << "the default binding picks the most recently analyzed architecture";
    ASSERT_NE(library.architecture("e", "a1"), nullptr);
    EXPECT_EQ(library.architecture("e", "a1")->name, "a1");
    EXPECT_EQ(library.architecture("e", "a3"), nullptr);
    EXPECT_EQ(library.latestArchitecture("missing"), nullptr);
}

TEST(Library_Elaboration, NamesDenoteTheirDeclarations)
{
    const ASTRoot file = parseSource(
        "entity e is generic (w : natural := 2); port (a : in std_logic; y : out std_logic); end e;"
        "architecture rtl of e is"
        "    type state_t is (idle, busy);"
        "    constant c : std_logic := '1';"
        "    signal s : state_t;"
        "begin"
        "    process (a)"
        "        variable v : std_logic;"
        "    begin"
        "        v := a and c;"
        "        for i in 0 to w loop s <= idle; end loop;"
        "        y <= v;"
        "    end process;"
        "end rtl;");
    DesignLibrary library;
    library.analyze(file);

    const auto* entity = TestUtil::nodeAs<EntityDeclaration>(file.children.at(0).get());
    const auto* arch = TestUtil::nodeAs<ArchitectureDeclaration>(file.children.at(1).get());
    const auto* process = TestUtil::nodeAs<ProcessStatement>(arch->body.at(0).get());

    const auto* variableAssignment = TestUtil::nodeAs<VariableAssignment>(process->body.at(0).get());
    const auto* andOp = TestUtil::nodeAs<BinaryOpExpr>(variableAssignment->value.get());
    const auto* a = TestUtil::nodeAs<SymbolExpr>(andOp->left.get());
    const auto* c = TestUtil::nodeAs<SymbolExpr>(andOp->right.get());
    const auto* v = TestUtil::nodeAs<SymbolExpr>(variableAssignment->target.get());
    EXPECT_EQ(library.declarationOf(*a), entity->ports.at(0).get());
    EXPECT_EQ(library.declarationOf(*c), arch->declarations.at(1).get());
    EXPECT_EQ(library.declarationOf(*v), process->declarations.at(0).get());

    const auto* loop = TestUtil::nodeAs<ForLoopStatement>(process->body.at(1).get());
    const auto* range = TestUtil::nodeAs<BinaryOpExpr>(loop->range.get());
    EXPECT_EQ(library.declarationOf(*TestUtil::nodeAs<SymbolExpr>(range->right.get())), entity->generics.at(0).get());

    const auto* inLoop = TestUtil::nodeAs<SignalAssignment>(loop->body.at(0).get());
    EXPECT_EQ(library.declarationOf(*TestUtil::nodeAs<SymbolExpr>(inLoop->target.get())), arch->declarations.at(2).get());
    EXPECT_EQ(library.declarationOf(*TestUtil::nodeAs<SymbolExpr>(inLoop->value.get())), nullptr) << "idle is a literal, not an object";
    ASSERT_NE(library.typeOf(*inLoop->value), nullptr);
    EXPECT_EQ(library.typeOf(*inLoop->value)->info->name, "state_t");

    const auto* signal = TestUtil::nodeAs<SignalDeclaration>(arch->declarations.at(2).get());
    ASSERT_NE(library.objectType(*signal), nullptr);
    EXPECT_EQ(library.objectType(*signal)->info->name, "state_t");
}
