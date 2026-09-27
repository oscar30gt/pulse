// subprograms.test.cc — function and procedure declarations and bodies, return and procedure calls.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    std::vector<DeclarationPtr> declarationsOf(const std::string& text)
    {
        Tokenizer tokenizer(text);
        return parseDeclarations(tokenizer);
    }

    const SubprogramSpec* specOf(const Declaration* declaration)
    {
        if (auto* body = dynamic_cast<const SubprogramBody*>(declaration))
            return body->spec.get();
        return as<SubprogramDeclaration>(declaration)->spec.get();
    }
}

// ===========================================================================
// 1. SPECIFICATIONS
// ===========================================================================

TEST(ParserSubprograms, FunctionDeclaration)
{
    auto declarations = declarationsOf("function add(a, b : in integer; c : integer := 0) return integer;");
    auto* spec = specOf(declarations.at(0).get());
    EXPECT_EQ(spec->kind, SubprogramKind::Function);
    EXPECT_FALSE(spec->impure);
    EXPECT_EQ(spec->name, "add");
    ASSERT_EQ(spec->parameters.size(), 3u);
    EXPECT_EQ(spec->parameters[1]->name, "b");
    EXPECT_EQ(spec->parameters[1]->objectClass, ParameterClass::Unspecified);
    EXPECT_EQ(render(spec->parameters[2]->defaultValue.get()), "0");
    EXPECT_EQ(render(spec->returnType.get()), "integer");
}

TEST(ParserSubprograms, PureAndImpure)
{
    auto declarations = declarationsOf("pure function f return bit; impure function g return bit; function h return bit;");
    ASSERT_EQ(declarations.size(), 3u);
    EXPECT_FALSE(specOf(declarations[0].get())->impure);
    EXPECT_TRUE(specOf(declarations[1].get())->impure);
    EXPECT_FALSE(specOf(declarations[2].get())->impure);
}

TEST(ParserSubprograms, ProcedureParameterClassesAndModes)
{
    auto declarations = declarationsOf(
        "procedure p(constant k : integer; signal s : out bit; variable v : inout integer; x : bit);");
    auto* spec = specOf(declarations.at(0).get());
    EXPECT_EQ(spec->kind, SubprogramKind::Procedure);
    EXPECT_EQ(spec->returnType, nullptr);
    ASSERT_EQ(spec->parameters.size(), 4u);
    EXPECT_EQ(spec->parameters[0]->objectClass, ParameterClass::Constant);
    EXPECT_EQ(spec->parameters[1]->objectClass, ParameterClass::Signal);
    EXPECT_EQ(spec->parameters[1]->mode, PortMode::Out);
    EXPECT_EQ(spec->parameters[2]->objectClass, ParameterClass::Variable);
    EXPECT_EQ(spec->parameters[2]->mode, PortMode::InOut);
    EXPECT_EQ(spec->parameters[3]->mode, PortMode::In);
}

TEST(ParserSubprograms, OptionalParameterKeywordAndNoParameters)
{
    EXPECT_EQ(specOf(declarationsOf("procedure p parameter (a : bit);").at(0).get())->parameters.size(), 1u);
    EXPECT_TRUE(specOf(declarationsOf("procedure p;").at(0).get())->parameters.empty());
}

TEST(ParserSubprograms, OperatorSymbolName)
{
    auto declarations = declarationsOf("function \"AND\"(l, r : bit) return bit is begin return l; end function \"and\";");
    EXPECT_EQ(specOf(declarations.at(0).get())->name, "\"and\"");
}

TEST(ParserSubprograms, SpecificationGrammar)
{
    for (const char* text : { "function f return;", "function f(a : bit);", "pure procedure p;", "procedure p return bit;",
                              "function f() return bit;", "procedure p(file f : text);" })
    {
        Tokenizer tokenizer(text);
        EXPECT_THROW((void)parseDeclarations(tokenizer), ast_syntax_error) << text;
    }
}

// ===========================================================================
// 2. BODIES
// ===========================================================================

TEST(ParserSubprograms, FunctionBody)
{
    auto declarations = declarationsOf(
        "function inc(x : integer) return integer is\n"
        "  variable t : integer;\n"
        "  constant one : integer := 1;\n"
        "begin\n"
        "  t := x + one;\n"
        "  return t;\n"
        "end function inc;");
    auto* body = as<SubprogramBody>(declarations.at(0).get());
    EXPECT_EQ(body->spec->name, "inc");
    EXPECT_EQ(body->declarations.size(), 2u);
    ASSERT_EQ(body->body.size(), 2u);
    auto* returned = as<ReturnStatement>(body->body[1].get());
    EXPECT_EQ(render(returned->value.get()), "t");
}

TEST(ParserSubprograms, ProcedureBodyWithEmptyReturn)
{
    auto declarations = declarationsOf("procedure p is begin return; end;");
    auto* body = as<SubprogramBody>(declarations.at(0).get());
    EXPECT_EQ(as<ReturnStatement>(body->body.at(0).get())->value, nullptr);
}

TEST(ParserSubprograms, BodyClosingForms)
{
    for (const char* ending : { "end;", "end procedure;", "end p;", "end procedure p;" })
        EXPECT_NO_THROW(declarationsOf(std::string("procedure p is begin ") + ending)) << ending;

    for (const char* ending : { "end function;", "end q;", "end procedure q;" })
    {
        Tokenizer tokenizer(std::string("procedure p is begin ") + ending);
        EXPECT_THROW((void)parseDeclarations(tokenizer), ast_syntax_error) << ending;
    }
}

TEST(ParserSubprograms, NestedSubprograms)
{
    auto declarations = declarationsOf(
        "function outer return bit is function inner return bit is begin return '1'; end; begin return inner; end;");
    auto* outer = as<SubprogramBody>(declarations.at(0).get());
    ASSERT_EQ(outer->declarations.size(), 1u);
    EXPECT_NE(dynamic_cast<const SubprogramBody*>(outer->declarations[0].get()), nullptr);
}

TEST(ParserSubprograms, SubprogramsInArchitecturesAndProcesses)
{
    EXPECT_NO_THROW(parseSource(inArchitecture("procedure p is begin null; end;")));
    EXPECT_NO_THROW(parseSource(inProcess("wait;", "function f return bit is begin return '0'; end;")));
}

// ===========================================================================
// 3. PROCEDURE CALLS
// ===========================================================================

TEST(ParserSubprograms, SequentialProcedureCalls)
{
    ASTRoot root = parseSource(inProcess("p; q(a, b => '1'); work.pkg.r(1);"));
    const auto& body = processBody(root);
    ASSERT_EQ(body.size(), 3u);
    EXPECT_EQ(render(as<ProcedureCallStatement>(body[0].get())->call.get()), "p");
    EXPECT_EQ(render(as<ProcedureCallStatement>(body[1].get())->call.get()), "q[a, b => '1']");
    EXPECT_EQ(render(as<ProcedureCallStatement>(body[2].get())->call.get()), "work.pkg.r[1]");
}

TEST(ParserSubprograms, ConcurrentProcedureCalls)
{
    ASTRoot root = parseSource(inArchitecture("", "check(clk); tick;"));
    const auto& body = architectureBody(root);
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(render(as<ProcedureCallStatement>(body[0].get())->call.get()), "check[clk]");
    EXPECT_EQ(render(as<ProcedureCallStatement>(body[1].get())->call.get()), "tick");
}

TEST(ParserSubprograms, ReturnOutsideASubprogramIsStillSyntax)
{
    // Where `return` may appear is a semantic rule; the parser only knows it is a sequential statement.
    EXPECT_NO_THROW(parseSource(inProcess("return;")));
    EXPECT_TRUE(syntaxError(inArchitecture("", "return;")).thrown);
}
