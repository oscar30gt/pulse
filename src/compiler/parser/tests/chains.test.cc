// chains.test.cc — long chains of operators, selections and alternatives.
//
// The parser builds a chain in a loop, so its length is not bounded by the stack; the tree it makes is that deep, though, and
// cloning, printing and the analyzer walk it recursively. The parser therefore stops a chain at 2000 links.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    constexpr int maxLinks = 2000;

    /// `operand separator operand separator ...` with `terms` operands.
    std::string chainOf(const std::string& operand, const std::string& separator, int terms)
    {
        std::string text = operand;
        for (int i = 1; i < terms; ++i)
            text += separator + operand;
        return text;
    }

    /// A process body with a single assignment of `value`.
    std::string assigning(const std::string& value)
    {
        return "entity e is end e; architecture r of e is signal n : integer; begin process begin n <= " + value + "; wait; end process; end r;";
    }

    bool accepted(const std::string& value)
    {
        return !syntaxError(assigning(value)).thrown;
    }

    bool tooLong(const std::string& value)
    {
        const SyntaxError error = syntaxError(assigning(value));
        return error.thrown && error.message.find("This expression is too long") != std::string::npos;
    }
}

// ---- Each kind of chain ----------------------------------------------------------------------

TEST(ParserChains, AdditionsAreLimited)
{
    EXPECT_TRUE(accepted(chainOf("a", " + ", maxLinks)));
    EXPECT_TRUE(tooLong(chainOf("a", " + ", maxLinks + 2)));
    EXPECT_TRUE(tooLong(chainOf("a", " - ", 100000)));
    EXPECT_TRUE(tooLong(chainOf("a", " & ", 100000)));
}

TEST(ParserChains, MultiplicationsAreLimited)
{
    EXPECT_TRUE(accepted(chainOf("a", " * ", maxLinks)));
    EXPECT_TRUE(tooLong(chainOf("a", " * ", maxLinks + 2)));
    EXPECT_TRUE(tooLong(chainOf("a", " mod ", 100000)));
}

TEST(ParserChains, LogicalChainsAreLimited)
{
    EXPECT_TRUE(accepted(chainOf("a", " and ", maxLinks)));
    EXPECT_TRUE(tooLong(chainOf("a", " and ", maxLinks + 2)));
    EXPECT_TRUE(tooLong(chainOf("a", " xor ", 100000)));
}

TEST(ParserChains, SelectionsAreLimited)
{
    EXPECT_TRUE(accepted("a" + chainOf(".b", "", maxLinks - 1)));
    EXPECT_TRUE(tooLong("a" + chainOf(".b", "", maxLinks + 1)));
    EXPECT_TRUE(tooLong("a" + chainOf(".b", "", 100000)));
}

TEST(ParserChains, IndexingsAreLimited)
{
    EXPECT_TRUE(accepted("a" + chainOf("(1)", "", maxLinks - 1)));
    EXPECT_TRUE(tooLong("a" + chainOf("(1)", "", maxLinks + 1)));
}

TEST(ParserChains, AttributesAreLimited)
{
    EXPECT_TRUE(accepted("a" + chainOf("'x", "", maxLinks - 1)));
    EXPECT_TRUE(tooLong("a" + chainOf("'x", "", maxLinks + 1)));
}

TEST(ParserChains, ConditionalAlternativesAreLimited)
{
    EXPECT_TRUE(accepted("1" + chainOf(" when true else 2", "", maxLinks - 1)));
    EXPECT_TRUE(tooLong("1" + chainOf(" when true else 2", "", maxLinks + 1)));
    EXPECT_TRUE(tooLong("1" + chainOf(" when true else 2", "", 100000)));
}

// ---- What counts as one chain ----------------------------------------------------------------

TEST(ParserChains, TheLimitIsPerPathNotPerFile)
{
    // Many long chains one after the other are fine: a chain frees its budget when it ends.
    std::string body;
    for (int i = 0; i < 20; ++i)
        body += "n <= " + chainOf("a", " + ", 1500) + "; ";
    EXPECT_FALSE(syntaxError("entity e is end e; architecture r of e is signal n : integer; begin process begin " + body + " wait; end process; end r;").thrown);
}

TEST(ParserChains, ChainsInsideOneAnotherAddUp)
{
    // Two chains along one path of the tree: 1500 alternatives whose last value is itself a chain of 1500 additions.
    const std::string nested = "1" + chainOf(" when true else 2", "", 1500) + " when true else " + chainOf("a", " + ", 1500);
    EXPECT_TRUE(tooLong(nested));
    EXPECT_TRUE(accepted("1" + chainOf(" when true else 2", "", 900) + " when true else " + chainOf("a", " + ", 900)));
}

TEST(ParserChains, ParenthesizedChainsStartAgain)
{
    // The left operand is complete before the outer chain begins, so it does not count towards it.
    EXPECT_TRUE(accepted("(" + chainOf("a", " + ", 1500) + ") + (" + chainOf("a", " + ", 1500) + ")"));
}

TEST(ParserChains, DifferentOperatorLevelsShareTheBudget)
{
    // `a * a * a ... + a * a * ...`: each product is a separate chain, and the sum above them counts one link each.
    std::string sum = chainOf("a * a", " + ", 1000);
    EXPECT_TRUE(accepted(sum));
    EXPECT_TRUE(tooLong(chainOf("a * a", " + ", 2500)));
}

TEST(ParserChains, ALongChainStillParsesToTheRightTree)
{
    const ASTRoot root = parseSource(assigning(chainOf("a", " + ", 1000)));
    const auto* assignment = as<SignalAssignment>(processBody(root).at(0).get());
    ASSERT_NE(assignment, nullptr);

    int links = 0;
    const Expression* node = assignment->value.get();
    while (const auto* sum = dynamic_cast<const BinaryOpExpr*>(node))
    {
        ++links;
        EXPECT_EQ(sum->op, BinaryOperator::Add);
        node = sum->left.get();
    }
    EXPECT_EQ(links, 999);
    EXPECT_NE(dynamic_cast<const SymbolExpr*>(node), nullptr);
}

TEST(ParserChains, ALongChainClonesAndPrints)
{
    const ASTRoot root = parseSource(assigning(chainOf("a", " + ", maxLinks)));
    const std::unique_ptr<ASTNode> copy = root.clone();
    EXPECT_EQ(dump(*copy), dump(root));
}

TEST(ParserChains, ChainsInDeclarationsAreCopiedForEveryName)
{
    // `constant p, q : integer := <chain>;` copies the value once per name.
    const std::string longest = chainOf("3", " + ", maxLinks);
    EXPECT_FALSE(syntaxError("entity e is end e; architecture r of e is constant p, q : integer := " + longest + "; begin end r;").thrown);

    const SyntaxError error = syntaxError("entity e is end e; architecture r of e is constant p, q : integer := " + chainOf("3", " + ", 100000) + "; begin end r;");
    EXPECT_TRUE(error.thrown);
    EXPECT_NE(error.message.find("This expression is too long"), std::string::npos) << error.message;
}

TEST(ParserChains, TheErrorPointsIntoTheChain)
{
    const SyntaxError error = syntaxError(assigning(chainOf("a", " + ", 5000)));
    ASSERT_TRUE(error.thrown);
    EXPECT_EQ(error.line, 1u);
    EXPECT_GT(error.column, 100u);
}
