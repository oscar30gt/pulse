// ast_printer.test.cc — the indentation-based AST dump.
//
// Nodes print themselves with print(indent); 1 indent unit is 4 spaces, colors are kept, and the
// operator names come from toString.

#include <gtest/gtest.h>

#include <sstream>
#include <iostream>
#include <string>
#include <vector>

#include "ast.h"

using namespace Pulse::Parser;

namespace
{
    /// Output of node.print(), captured by redirecting std::cout (works on every toolchain).
    std::string dump(const ASTNode& node, int indent = 0)
    {
        std::ostringstream captured;
        std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
        node.print(indent);
        std::cout.rdbuf(previous);
        return captured.str();
    }

    std::vector<std::string> lines(const std::string& text)
    {
        std::vector<std::string> result;
        std::istringstream stream(text);
        for (std::string line; std::getline(stream, line);)
            result.push_back(line);
        return result;
    }

    ExpressionPtr sym(const std::string& name)
    {
        auto e = std::make_unique<SymbolExpr>();
        e->name = name;
        return e;
    }

    /// `a + b` inside a signal assignment inside a process: three levels of nesting.
    std::unique_ptr<ProcessStatement> sampleProcess()
    {
        auto sum = std::make_unique<BinaryOpExpr>();
        sum->op = BinaryOperator::Add;
        sum->left = sym("a");
        sum->right = sym("b");

        auto assignment = std::make_unique<SignalAssignment>();
        assignment->target = sym("y");
        assignment->value = std::move(sum);

        auto process = std::make_unique<ProcessStatement>();
        process->label = "p1";
        process->body.push_back(std::move(assignment));
        return process;
    }
}

TEST(AstPrinter, IndentIsFourSpacesPerUnit)
{
    auto process = sampleProcess();

    const auto flat = lines(dump(*process, 0));
    const auto shifted = lines(dump(*process, 2));

    ASSERT_EQ(flat.size(), shifted.size());
    ASSERT_GT(flat.size(), 3u);
    for (size_t i = 0; i < flat.size(); ++i)
        EXPECT_EQ(shifted[i], std::string(8, ' ') + flat[i]) << "line " << i;
}

TEST(AstPrinter, ChildrenAreIndentedDeeperThanTheirParent)
{
    auto process = sampleProcess();
    const auto out = lines(dump(*process));

    auto indentOf = [](const std::string& line) { return line.find_first_not_of(' '); };

    EXPECT_EQ(indentOf(out.front()), 0u) << "the node itself is at the requested indent";
    for (size_t i = 1; i < out.size(); ++i)
        EXPECT_GE(indentOf(out[i]), 4u) << "line " << i << ": " << out[i];
    EXPECT_EQ(out.front().find("PROCESS"), out.front().find_first_not_of(' ') + std::string("\033[1;36m").size());
}

TEST(AstPrinter, ShowsLabelsNamesAndOperators)
{
    auto process = sampleProcess();
    const std::string out = dump(*process);

    EXPECT_NE(out.find("p1"), std::string::npos);
    EXPECT_NE(out.find("BINARY OP"), std::string::npos);
    EXPECT_NE(out.find("+"), std::string::npos);
    EXPECT_NE(out.find("SYMBOL"), std::string::npos);
}

TEST(AstPrinter, KeepsAnsiColors)
{
    auto process = sampleProcess();
    const std::string out = dump(*process);

    EXPECT_NE(out.find("\033[1;36m"), std::string::npos) << "keyword color";
    EXPECT_NE(out.find("\033[32m"), std::string::npos) << "identifier color";
    EXPECT_NE(out.find("\033[0m"), std::string::npos) << "reset";
}

TEST(AstPrinter, NullChildrenPrintAPlaceholder)
{
    WhenElseExpr node;
    node.trueValue = sym("a");

    const std::string out = dump(node);

    EXPECT_NE(out.find("<none>"), std::string::npos);
}

TEST(AstPrinter, StringsPrintOnOneLine)
{
    StringLiteralExpr literal;
    literal.value = "101";

    const std::string out = dump(literal);

    EXPECT_EQ(lines(out).size(), 1u);
    EXPECT_NE(out.find("\"101\""), std::string::npos);
}

TEST(AstPrinter, WhenElseChainNestsThroughTheElseField)
{
    auto inner = std::make_unique<WhenElseExpr>();
    inner->trueValue = sym("b");
    inner->condition = sym("c2");
    inner->falseValue = sym("d");

    WhenElseExpr outer;
    outer.trueValue = sym("a");
    outer.condition = sym("c1");
    outer.falseValue = std::move(inner);

    const std::string out = dump(outer);
    const auto firstWhenElse = out.find("WHEN ELSE");
    const auto secondWhenElse = out.find("WHEN ELSE", firstWhenElse + 1);

    EXPECT_NE(firstWhenElse, std::string::npos);
    EXPECT_NE(secondWhenElse, std::string::npos) << "the second branch is printed as a nested WHEN ELSE";
}

TEST(AstPrinter, OperatorSpellings)
{
    EXPECT_STREQ(toString(BinaryOperator::Downto), "downto");
    EXPECT_STREQ(toString(BinaryOperator::MatchNeq), "?/=");
    EXPECT_STREQ(toString(BinaryOperator::Pow), "**");
    EXPECT_STREQ(toString(UnaryOperator::Condition), "??");
    EXPECT_STREQ(toString(UnaryOperator::Not), "not");
}

TEST(AstPrinter, RootPrintsItsDesignUnits)
{
    ASTRoot root;
    auto entity = std::make_unique<EntityDeclaration>();
    entity->name = "top";
    root.children.push_back(std::move(entity));

    const std::string out = dump(root);

    EXPECT_NE(out.find("ENTITY"), std::string::npos);
    EXPECT_NE(out.find("top"), std::string::npos);
}

TEST(AstPrinter, EveryEntityClassAndPortModeHasItsSourceSpelling)
{
    const std::vector<std::pair<EntityClass, std::string>> classes = {
        { EntityClass::Entity, "entity" }, { EntityClass::Architecture, "architecture" }, { EntityClass::Configuration, "configuration" },
        { EntityClass::Procedure, "procedure" }, { EntityClass::Function, "function" }, { EntityClass::Package, "package" },
        { EntityClass::Type, "type" }, { EntityClass::Subtype, "subtype" }, { EntityClass::Constant, "constant" },
        { EntityClass::Signal, "signal" }, { EntityClass::Variable, "variable" }, { EntityClass::Component, "component" },
        { EntityClass::Label, "label" }, { EntityClass::Literal, "literal" }, { EntityClass::Units, "units" },
        { EntityClass::Group, "group" }, { EntityClass::File, "file" }, { EntityClass::Property, "property" },
        { EntityClass::Sequence, "sequence" },
    };
    for (const auto& [entityClass, spelling] : classes)
        EXPECT_EQ(toString(entityClass), spelling);

    EXPECT_STREQ(toString(PortMode::In), "in");
    EXPECT_STREQ(toString(PortMode::Out), "out");
    EXPECT_STREQ(toString(PortMode::InOut), "inout");
}

TEST(AstPrinter, ParameterClassesArePrintedAsWritten)
{
    const std::vector<std::pair<ParameterClass, std::string>> classes = {
        { ParameterClass::Constant, "constant" }, { ParameterClass::Signal, "signal" }, { ParameterClass::Variable, "variable" },
    };
    for (const auto& [objectClass, spelling] : classes)
    {
        ParameterDeclaration parameter;
        parameter.name = "x";
        parameter.objectClass = objectClass;
        EXPECT_NE(dump(parameter).find(spelling), std::string::npos) << spelling;
    }

    ParameterDeclaration plain;
    plain.name = "x";
    const std::string text = dump(plain);
    for (const char* word : { "constant", "signal", "variable" })
        EXPECT_EQ(text.find(word), std::string::npos) << word << " must not appear for an unspecified class";
}

TEST(AstPrinter, ExternalNamesPrintTheirObjectClass)
{
    for (const auto& [objectClass, spelling] : { std::pair{ ExternalObjectClass::Constant, "constant" },
                                                 std::pair{ ExternalObjectClass::Signal, "signal" },
                                                 std::pair{ ExternalObjectClass::Variable, "variable" } })
    {
        ExternalNameExpr external;
        external.objectClass = objectClass;
        external.path = ".a.b";
        EXPECT_NE(dump(external).find(spelling), std::string::npos) << spelling;
        EXPECT_NE(dump(external).find(".a.b"), std::string::npos);
    }
}
