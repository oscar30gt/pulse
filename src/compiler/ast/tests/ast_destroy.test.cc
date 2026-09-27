// ast_destroy.test.cc — chains of expressions are destroyed without recursion.
//
// A chain of additions `a + b + c + ...` nests through the left operand of each BinaryOpExpr, a selection `a.b.c` through the
// target, a call `f(1)(2)` through the callee, an attribute `a'x'y` through the prefix and a conditional value through its
// falseValue. The parser stops a chain at 2000 links, but a tree can be built by hand (or by a future front end) and any length
// must be safe to free: a recursive destructor would take one stack frame per link and overflow long before 500 000.

#include <gtest/gtest.h>

#include "ast.h"

using namespace Pulse::Parser;

namespace
{
    constexpr int links = 500000;

    ExpressionPtr symbol(const std::string& name)
    {
        auto node = std::make_unique<SymbolExpr>();
        node->name = name;
        return node;
    }

    ExpressionPtr additions(int count)
    {
        ExpressionPtr chain = symbol("a");
        for (int i = 0; i < count; ++i)
        {
            auto sum = std::make_unique<BinaryOpExpr>();
            sum->op = BinaryOperator::Add;
            sum->left = std::move(chain);
            sum->right = symbol("b");
            chain = std::move(sum);
        }
        return chain;
    }

    ExpressionPtr selections(int count)
    {
        ExpressionPtr chain = symbol("a");
        for (int i = 0; i < count; ++i)
        {
            auto field = std::make_unique<FieldAccessExpr>();
            field->target = std::move(chain);
            field->fieldName = "f";
            chain = std::move(field);
        }
        return chain;
    }

    ExpressionPtr calls(int count)
    {
        ExpressionPtr chain = symbol("a");
        for (int i = 0; i < count; ++i)
        {
            auto call = std::make_unique<FunctionCallExpr>();
            call->callee = std::move(chain);
            call->arguments.push_back(symbol("i"));
            chain = std::move(call);
        }
        return chain;
    }

    ExpressionPtr attributes(int count)
    {
        ExpressionPtr chain = symbol("a");
        for (int i = 0; i < count; ++i)
        {
            auto attribute = std::make_unique<AttributeExpr>();
            attribute->prefix = std::move(chain);
            attribute->attributeName = "x";
            chain = std::move(attribute);
        }
        return chain;
    }

    ExpressionPtr alternatives(int count)
    {
        ExpressionPtr chain = symbol("last");
        for (int i = 0; i < count; ++i)
        {
            auto branch = std::make_unique<WhenElseExpr>();
            branch->trueValue = symbol("v");
            branch->condition = symbol("c");
            branch->falseValue = std::move(chain);
            chain = std::move(branch);
        }
        return chain;
    }

    /// Every kind of link in one chain, in turn.
    ExpressionPtr mixed(int count)
    {
        ExpressionPtr chain = symbol("a");
        for (int i = 0; i < count; ++i)
        {
            switch (i % 5)
            {
                case 0:
                {
                    auto sum = std::make_unique<BinaryOpExpr>();
                    sum->left = std::move(chain);
                    sum->right = symbol("b");
                    chain = std::move(sum);
                    break;
                }
                case 1:
                {
                    auto field = std::make_unique<FieldAccessExpr>();
                    field->target = std::move(chain);
                    chain = std::move(field);
                    break;
                }
                case 2:
                {
                    auto call = std::make_unique<FunctionCallExpr>();
                    call->callee = std::move(chain);
                    chain = std::move(call);
                    break;
                }
                case 3:
                {
                    auto attribute = std::make_unique<AttributeExpr>();
                    attribute->prefix = std::move(chain);
                    chain = std::move(attribute);
                    break;
                }
                default:
                {
                    auto branch = std::make_unique<WhenElseExpr>();
                    branch->trueValue = symbol("v");
                    branch->condition = symbol("c");
                    branch->falseValue = std::move(chain);
                    chain = std::move(branch);
                    break;
                }
            }
        }
        return chain;
    }
}

TEST(AstDestroy, LongChainsOfAdditionsAreFreed)      { EXPECT_NO_FATAL_FAILURE(additions(links).reset()); }
TEST(AstDestroy, LongChainsOfSelectionsAreFreed)     { EXPECT_NO_FATAL_FAILURE(selections(links).reset()); }
TEST(AstDestroy, LongChainsOfCallsAreFreed)          { EXPECT_NO_FATAL_FAILURE(calls(links).reset()); }
TEST(AstDestroy, LongChainsOfAttributesAreFreed)     { EXPECT_NO_FATAL_FAILURE(attributes(links).reset()); }
TEST(AstDestroy, LongChainsOfAlternativesAreFreed)   { EXPECT_NO_FATAL_FAILURE(alternatives(links).reset()); }
TEST(AstDestroy, ChainsOfMixedLinksAreFreed)         { EXPECT_NO_FATAL_FAILURE(mixed(links).reset()); }

TEST(AstDestroy, AChainInsideAStatementIsFreedWithIt)
{
    auto assignment = std::make_unique<SignalAssignment>();
    assignment->target = symbol("n");
    assignment->value = additions(links);
    EXPECT_NO_FATAL_FAILURE(assignment.reset());
}

TEST(AstDestroy, AChainInTheOperandOfAnotherIsFreed)
{
    // The spine is followed through the left operand only; a chain hanging off the right one is freed by the destructor of the
    // node that owns it, which then walks that chain iteratively.
    auto outer = std::make_unique<BinaryOpExpr>();
    outer->left = additions(1000);
    outer->right = additions(links);
    EXPECT_NO_FATAL_FAILURE(outer.reset());
}

TEST(AstDestroy, UnlinkSpineHandlesEmptyAndSingleNodes)
{
    ExpressionPtr none;
    unlinkSpine(none);
    EXPECT_EQ(none, nullptr);

    ExpressionPtr single = symbol("a");
    unlinkSpine(single);
    EXPECT_EQ(single, nullptr) << "the head is always taken";

    ExpressionPtr open = std::make_unique<BinaryOpExpr>();
    unlinkSpine(open);
    EXPECT_EQ(open, nullptr) << "a node whose spine child is empty";
}

TEST(AstDestroy, OnlyChainNodesHaveASpineChild)
{
    EXPECT_EQ(symbol("a")->spineChild(), nullptr);

    ExpressionPtr sum = additions(1);
    ASSERT_NE(sum->spineChild(), nullptr);
    EXPECT_EQ(sum->spineChild()->get(), static_cast<BinaryOpExpr*>(sum.get())->left.get());

    EXPECT_NE(selections(1)->spineChild(), nullptr);
    EXPECT_NE(calls(1)->spineChild(), nullptr);
    EXPECT_NE(attributes(1)->spineChild(), nullptr);
    EXPECT_NE(alternatives(1)->spineChild(), nullptr);

    EXPECT_EQ(UnaryOpExpr().spineChild(), nullptr);
}
