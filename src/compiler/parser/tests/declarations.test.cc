// declarations.test.cc — objects, components, aliases, attributes and the per-region grammar.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    std::vector<DeclarationPtr> declarationsOf(const std::string& text)
    {
        Tokenizer tokenizer(text);
        return parseDeclarations(tokenizer);
    }
}

// ===========================================================================
// 1. OBJECTS
// ===========================================================================

TEST(ParserDeclarations, SignalWithInitialValue)
{
    auto declarations = declarationsOf("signal a, b : std_logic := '0';");
    ASSERT_EQ(declarations.size(), 2u);
    auto* b = as<SignalDeclaration>(declarations[1].get());
    EXPECT_EQ(b->name, "b");
    EXPECT_EQ(render(b->typeSpec.get()), "std_logic");
    EXPECT_EQ(render(b->initialValue.get()), "'0'");
}

TEST(ParserDeclarations, SignalWithoutInitialValue)
{
    auto declarations = declarationsOf("signal s : unsigned(3 downto 0);");
    auto* s = as<SignalDeclaration>(declarations.at(0).get());
    EXPECT_EQ(s->initialValue, nullptr);
    EXPECT_EQ(render(s->typeSpec.get()), "unsigned((3 downto 0))");
}

TEST(ParserDeclarations, ConstantNeedsAValue)
{
    auto declarations = declarationsOf("constant width : natural := 8 * 2;");
    EXPECT_EQ(render(as<ConstantDeclaration>(declarations.at(0).get())->value.get()), "(8 * 2)");

    Tokenizer deferred("constant c : natural;");
    EXPECT_THROW((void)parseDeclarations(deferred), ast_syntax_error);
}

TEST(ParserDeclarations, VariablesInProcessesAndSubprograms)
{
    ASTRoot root = parseSource(inProcess("wait;", "variable v, w : integer := 3;"));
    auto* process = as<ProcessStatement>(architectureBody(root).at(0).get());
    ASSERT_EQ(process->declarations.size(), 2u);
    EXPECT_EQ(render(as<VariableDeclaration>(process->declarations[1].get())->initialValue.get()), "3");
}

TEST(ParserDeclarations, SignalKindsAreNotSupported)
{
    Tokenizer tokenizer("signal s : bit register;");
    EXPECT_THROW((void)parseDeclarations(tokenizer), ast_syntax_error);
}

// ===========================================================================
// 2. REGIONS
// ===========================================================================

TEST(ParserDeclarations, ArchitectureRejectsVariables)
{
    SyntaxError error = syntaxError(inArchitecture("variable v : integer;"));
    ASSERT_TRUE(error.thrown);
    EXPECT_NE(error.message.find("Expected 'begin', but found 'variable'"), std::string::npos) << error.message;
}

TEST(ParserDeclarations, ProcessRejectsSignalsAndComponents)
{
    EXPECT_TRUE(syntaxError(inProcess("wait;", "signal s : bit;")).thrown);
    EXPECT_TRUE(syntaxError(inProcess("wait;", "component c end component;")).thrown);
}

TEST(ParserDeclarations, SubprogramRejectsSignals)
{
    EXPECT_TRUE(syntaxError(inArchitecture("procedure p is signal s : bit; begin end;")).thrown);
}

TEST(ParserDeclarations, NoDeclarationsAfterBegin)
{
    EXPECT_TRUE(syntaxError(inProcess("variable v : integer;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "signal s : bit;")).thrown);
}

TEST(ParserDeclarations, StandaloneDeclarationsStopOnlyAtTheEnd)
{
    auto declarations = declarationsOf("type t is (a, b); subtype s is t; constant c : t := a;");
    EXPECT_EQ(declarations.size(), 3u);

    Tokenizer trailing("constant c : integer := 1; begin");
    EXPECT_THROW((void)parseDeclarations(trailing), ast_syntax_error);
}

// ===========================================================================
// 3. COMPONENTS
// ===========================================================================

TEST(ParserDeclarations, ComponentWithGenericsAndPorts)
{
    auto declarations = declarationsOf(
        "component adder is generic (n : natural := 4); port (a, b : in bit; s : out bit); end component adder;");
    auto* component = as<ComponentDeclaration>(declarations.at(0).get());
    EXPECT_EQ(component->name, "adder");
    EXPECT_EQ(component->generics.size(), 1u);
    ASSERT_EQ(component->ports.size(), 3u);
    EXPECT_EQ(component->ports[2]->mode, PortMode::Out);
}

TEST(ParserDeclarations, ComponentClosing)
{
    EXPECT_NO_THROW(declarationsOf("component c end component;"));
    Tokenizer wrongName("component c end component d;");
    EXPECT_THROW((void)parseDeclarations(wrongName), ast_syntax_error);
    Tokenizer missingKind("component c end;");
    EXPECT_THROW((void)parseDeclarations(missingKind), ast_syntax_error);
}

// ===========================================================================
// 4. ALIASES AND ATTRIBUTES
// ===========================================================================

TEST(ParserDeclarations, AliasOfAnObject)
{
    auto declarations = declarationsOf("alias high : bit_vector(3 downto 0) is word(7 downto 4);");
    auto* alias = as<AliasDeclaration>(declarations.at(0).get());
    EXPECT_EQ(alias->name, "high");
    EXPECT_EQ(render(alias->subtype.get()), "bit_vector((3 downto 0))");
    EXPECT_EQ(render(alias->target.get()), "word[(7 downto 4)]");
    EXPECT_EQ(alias->signature, nullptr);
}

TEST(ParserDeclarations, AliasOfASubprogramWithSignature)
{
    auto declarations = declarationsOf("alias to_int is to_integer [unsigned return natural];");
    auto* alias = as<AliasDeclaration>(declarations.at(0).get());
    EXPECT_EQ(alias->subtype, nullptr);
    EXPECT_EQ(render(alias->target.get()), "to_integer");
    ASSERT_NE(alias->signature, nullptr);
    ASSERT_EQ(alias->signature->parameters.size(), 1u);
    EXPECT_EQ(render(alias->signature->parameters[0].get()), "unsigned");
    EXPECT_EQ(render(alias->signature->returnType.get()), "natural");
}

TEST(ParserDeclarations, AliasOfAnOperatorOrCharacter)
{
    auto declarations = declarationsOf("alias \"AND\" is my_and [bit, bit return bit]; alias '1' is one;");
    EXPECT_EQ(as<AliasDeclaration>(declarations.at(0).get())->name, "\"and\"");
    EXPECT_EQ(as<AliasDeclaration>(declarations.at(1).get())->name, "'1'");
}

TEST(ParserDeclarations, AttributeDeclaration)
{
    auto declarations = declarationsOf("attribute keep : boolean;");
    auto* attribute = as<AttributeDeclaration>(declarations.at(0).get());
    EXPECT_EQ(attribute->name, "keep");
    EXPECT_EQ(render(attribute->typeMark.get()), "boolean");
}

TEST(ParserDeclarations, AttributeSpecification)
{
    auto declarations = declarationsOf(
        "attribute keep of s1, s2 : signal is true;"
        "attribute loc of others : label is \"X1\";"
        "attribute dont_touch of all : component is 1 + 1;");
    ASSERT_EQ(declarations.size(), 3u);

    auto* first = as<AttributeSpecification>(declarations[0].get());
    EXPECT_EQ(first->attributeName, "keep");
    ASSERT_EQ(first->entities.size(), 2u);
    EXPECT_EQ(render(first->entities[1].get()), "s2");
    EXPECT_EQ(first->entityClass, EntityClass::Signal);
    EXPECT_EQ(render(first->value.get()), "true");

    auto* second = as<AttributeSpecification>(declarations[1].get());
    EXPECT_NE(dynamic_cast<const OthersExpr*>(second->entities.at(0).get()), nullptr);
    EXPECT_EQ(second->entityClass, EntityClass::Label);

    auto* third = as<AttributeSpecification>(declarations[2].get());
    EXPECT_NE(dynamic_cast<const AllExpr*>(third->entities.at(0).get()), nullptr);
    EXPECT_EQ(third->entityClass, EntityClass::Component);
}

TEST(ParserDeclarations, AttributeSpecificationNeedsAnEntityClass)
{
    Tokenizer tokenizer("attribute keep of s : wire is true;");
    EXPECT_THROW((void)parseDeclarations(tokenizer), ast_syntax_error);
}

TEST(ParserDeclarations, UseClauseInADeclarativePart)
{
    ASTRoot root = parseSource(inArchitecture("use work.pkg.all;"));
    auto* use = as<UseClause>(firstArchitecture(root)->declarations.at(0).get());
    EXPECT_EQ(render(use->names.at(0).get()), "work.pkg.all");
}

TEST(ParserDeclarations, UnsupportedDeclarationsAreSyntaxErrors)
{
    for (const char* declaration : { "shared variable v : integer;", "file f : text;", "group g : t (a);",
                                     "disconnect s : bit after 1 ns;" })
        EXPECT_TRUE(syntaxError(inArchitecture(declaration)).thrown) << declaration;
}
