// design_units.test.cc — entities, architectures and their context clauses.

#include "parser_test_util.h"

using namespace ParserTest;

// ===========================================================================
// 1. FILES
// ===========================================================================

TEST(ParserDesignUnits, EmptyFileHasNoUnits)
{
    ASTRoot root = parseSource("  -- only a comment\n");
    EXPECT_TRUE(root.children.empty());
}

TEST(ParserDesignUnits, UnitsKeepSourceOrder)
{
    ASTRoot root = parseSource("entity a is end; architecture r of a is begin end; entity b is end;");
    ASSERT_EQ(root.children.size(), 3u);
    EXPECT_EQ(as<EntityDeclaration>(root.children[0].get())->name, "a");
    EXPECT_EQ(as<ArchitectureDeclaration>(root.children[1].get())->name, "r");
    EXPECT_EQ(as<EntityDeclaration>(root.children[2].get())->name, "b");
}

TEST(ParserDesignUnits, UnsupportedUnitsAreSyntaxErrors)
{
    for (const char* source : { "package p is end;", "configuration c of e is for a end for; end;", "context c is end;" })
    {
        SyntaxError error = syntaxError(source);
        EXPECT_TRUE(error.thrown) << source;
        EXPECT_NE(error.message.find("Expected 'entity' or 'architecture'"), std::string::npos) << error.message;
    }
}

TEST(ParserDesignUnits, IdentifiersAreLowercased)
{
    ASTRoot root = parseSource("ENTITY Top IS END ENTITY TOP;");
    EXPECT_EQ(as<EntityDeclaration>(root.children.at(0).get())->name, "top");
}

// ===========================================================================
// 2. ENTITIES
// ===========================================================================

TEST(ParserDesignUnits, EntityClosingForms)
{
    for (const char* ending : { "end;", "end entity;", "end e;", "end entity e;" })
        EXPECT_NO_THROW(parseSource(std::string("entity e is ") + ending)) << ending;
}

TEST(ParserDesignUnits, WrongClosingNameIsRejectedAtTheName)
{
    SyntaxError error = syntaxError("entity e is\nend entity f;");
    ASSERT_TRUE(error.thrown);
    EXPECT_NE(error.message.find("does not match 'e'"), std::string::npos) << error.message;
    EXPECT_EQ(error.line, 2u);
    EXPECT_EQ(error.column, 12u);
}

TEST(ParserDesignUnits, EntityPorts)
{
    ASTRoot root = parseSource(
        "entity e is port (a, b : in bit; y : out bit_vector(7 downto 0) := x\"00\"; z : inout bit;\n"
        "                  d : bit); end;");
    auto* entity = as<EntityDeclaration>(root.children.at(0).get());
    ASSERT_EQ(entity->ports.size(), 5u);

    EXPECT_EQ(entity->ports[0]->name, "a");
    EXPECT_EQ(entity->ports[1]->name, "b");
    EXPECT_EQ(entity->ports[1]->mode, PortMode::In);
    EXPECT_EQ(entity->ports[2]->mode, PortMode::Out);
    EXPECT_EQ(render(entity->ports[2]->typeSpec.get()), "bit_vector((7 downto 0))");
    EXPECT_EQ(render(entity->ports[2]->defaultValue.get()), "\"00000000\"");
    EXPECT_EQ(entity->ports[3]->mode, PortMode::InOut);
    EXPECT_EQ(entity->ports[4]->mode, PortMode::In) << "the mode defaults to in";
    EXPECT_EQ(entity->ports[4]->defaultValue, nullptr);
}

TEST(ParserDesignUnits, BufferAndLinkageModesAreNotSupported)
{
    SyntaxError buffer = syntaxError("entity e is port (q : buffer bit); end;");
    ASSERT_TRUE(buffer.thrown);
    EXPECT_NE(buffer.message.find("Expected an identifier, but found 'buffer'"), std::string::npos) << buffer.message;
    EXPECT_TRUE(syntaxError("entity e is port (q : linkage bit); end;").thrown);
    EXPECT_TRUE(syntaxError("architecture a of e is procedure p(x : buffer bit); begin end;").thrown);
}

TEST(ParserDesignUnits, NamesOfOneGroupGetTheirOwnTypeCopy)
{
    ASTRoot root = parseSource("entity e is port (a, b : in bit); end;");
    auto* entity = as<EntityDeclaration>(root.children.at(0).get());
    ASSERT_EQ(entity->ports.size(), 2u);
    EXPECT_NE(entity->ports[0]->typeSpec.get(), entity->ports[1]->typeSpec.get());
    EXPECT_EQ(entity->ports[1]->source.column, 22u) << "each port is located at its own name";
}

TEST(ParserDesignUnits, EntityGenerics)
{
    ASTRoot root = parseSource("entity e is generic (n : natural := 8; w, h : integer); port (a : bit); end;");
    auto* entity = as<EntityDeclaration>(root.children.at(0).get());
    ASSERT_EQ(entity->generics.size(), 3u);
    EXPECT_EQ(entity->generics[0]->name, "n");
    EXPECT_EQ(render(entity->generics[0]->typeSpec.get()), "natural");
    EXPECT_EQ(render(entity->generics[0]->defaultValue.get()), "8");
    EXPECT_EQ(entity->generics[2]->name, "h");
    EXPECT_EQ(entity->generics[2]->defaultValue, nullptr);
    EXPECT_EQ(entity->ports.size(), 1u);
}

TEST(ParserDesignUnits, EntityClausesMustBeInOrderAndTerminated)
{
    EXPECT_TRUE(syntaxError("entity e is port (a : bit); generic (n : natural); end;").thrown);
    EXPECT_TRUE(syntaxError("entity e is port (a : bit) end;").thrown);
    EXPECT_TRUE(syntaxError("entity e is port (a : bit;); end;").thrown);
    EXPECT_TRUE(syntaxError("entity e is port (); end;").thrown);
}

TEST(ParserDesignUnits, EntityStatementPartIsNotSupported)
{
    SyntaxError error = syntaxError("entity e is begin end;");
    EXPECT_NE(error.message.find("Expected 'end', but found 'begin'"), std::string::npos) << error.message;
}

// ===========================================================================
// 3. ARCHITECTURES
// ===========================================================================

TEST(ParserDesignUnits, ArchitectureParts)
{
    ASTRoot root = parseSource(
        "architecture rtl of top is\n"
        "  signal s : bit;\n"
        "  constant c : integer := 1;\n"
        "begin\n"
        "  s <= '1';\n"
        "  p : process begin wait; end process;\n"
        "end architecture rtl;");
    auto* architecture = as<ArchitectureDeclaration>(root.children.at(0).get());
    EXPECT_EQ(architecture->name, "rtl");
    EXPECT_EQ(architecture->entityName, "top");
    EXPECT_EQ(architecture->declarations.size(), 2u);
    EXPECT_EQ(architecture->body.size(), 2u);
    EXPECT_EQ(architecture->source.line, 1u);
    EXPECT_EQ(architecture->source.column, 1u);
}

TEST(ParserDesignUnits, ArchitectureClosingForms)
{
    for (const char* ending : { "end;", "end architecture;", "end a;", "end architecture a;" })
        EXPECT_NO_THROW(parseSource(std::string("architecture a of e is begin ") + ending)) << ending;
    EXPECT_TRUE(syntaxError("architecture a of e is begin end architecture b;").thrown);
    EXPECT_TRUE(syntaxError("architecture a of e is begin end entity;").thrown);
}

TEST(ParserDesignUnits, ArchitectureNeedsBegin)
{
    EXPECT_TRUE(syntaxError("architecture a of e is end;").thrown);
    EXPECT_TRUE(syntaxError("architecture a of e begin end;").thrown);
}

// ===========================================================================
// 4. CONTEXT CLAUSES
// ===========================================================================

TEST(ParserDesignUnits, ContextClauseBelongsToTheNextUnit)
{
    ASTRoot root = parseSource(
        "library ieee, work;\n"
        "use ieee.std_logic_1164.all, ieee.numeric_std.unsigned;\n"
        "entity e is end;\n"
        "architecture a of e is begin end;");
    ASSERT_EQ(root.children.size(), 2u);

    const auto& context = root.children[0]->context;
    ASSERT_EQ(context.size(), 2u);
    auto* library = as<LibraryClause>(context[0].get());
    EXPECT_EQ(library->names, (std::vector<std::string>{ "ieee", "work" }));
    auto* use = as<UseClause>(context[1].get());
    ASSERT_EQ(use->names.size(), 2u);
    EXPECT_EQ(render(use->names[0].get()), "ieee.std_logic_1164.all");
    EXPECT_EQ(render(use->names[1].get()), "ieee.numeric_std.unsigned");

    EXPECT_TRUE(root.children[1]->context.empty());
}

TEST(ParserDesignUnits, ContextClauseNeedsAUnit)
{
    SyntaxError error = syntaxError("library ieee;");
    EXPECT_NE(error.message.find("but found the end of the file"), std::string::npos) << error.message;
}

TEST(ParserDesignUnits, UseClauseNeedsASelectedName)
{
    EXPECT_TRUE(syntaxError("use ieee; entity e is end;").thrown);
    EXPECT_TRUE(syntaxError("use ieee.; entity e is end;").thrown);
    EXPECT_TRUE(syntaxError("use ieee.all.x entity e is end;").thrown);
}

TEST(ParserDesignUnits, EntityPrintsItsContext)
{
    ASTRoot root = parseSource("library ieee; entity e is end;");
    EXPECT_NE(dump(*root.children[0]).find("LIBRARY"), std::string::npos);
}
