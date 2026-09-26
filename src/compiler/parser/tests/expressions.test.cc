// expressions.test.cc — precedence, operators, names and aggregates, asserted through TestUtil::render.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    bool rejects(const std::string& expression)
    {
        return syntaxError(inArchitecture("", "y <= " + expression + ";")).thrown;
    }

    std::string errorOf(const std::string& expression)
    {
        return syntaxError(inArchitecture("", "y <= " + expression + ";")).message;
    }
}

// ===========================================================================
// 1. PRECEDENCE
// ===========================================================================

TEST(ParserExpressions, MultiplyingBindsTighterThanAdding)
{
    EXPECT_EQ(renderValue("a + b * c"), "(a + (b * c))");
    EXPECT_EQ(renderValue("a * b + c"), "((a * b) + c)");
    EXPECT_EQ(renderValue("a - b - c"), "((a - b) - c)") << "left associative";
    EXPECT_EQ(renderValue("a mod b rem c / d"), "(((a mod b) rem c) / d)");
}

TEST(ParserExpressions, FullLadder)
{
    // relational < shift < adding < multiplying < misc
    EXPECT_EQ(renderValue("a & b sll 1 = c ** 2 * d"), "(((a & b) sll 1) = ((c ** 2) * d))");
    EXPECT_EQ(renderValue("a = b and c /= d"), "((a = b) and (c /= d))");
}

TEST(ParserExpressions, EveryShiftOperatorSitsBetweenRelationalAndAdding)
{
    for (const std::string op : { "sll", "srl", "sla", "sra", "rol", "ror" })
    {
        EXPECT_EQ(renderValue("a " + op + " b + 1 = c"), "((a " + op + " (b + 1)) = c)") << op;
        EXPECT_TRUE(rejects("a " + op + " 1 " + op + " 2")) << op << " cannot be chained without parentheses";
    }
}

TEST(ParserExpressions, SignAppliesToTheFirstTerm)
{
    EXPECT_EQ(renderValue("-a * b + c"), "((- (a * b)) + c)");
    EXPECT_EQ(renderValue("+a"), "(+ a)");
    EXPECT_TRUE(rejects("a + -b")) << "VHDL needs parentheses: a + (-b)";
    EXPECT_EQ(renderValue("a + (-b)"), "(a + (- b))");
}

TEST(ParserExpressions, MiscellaneousOperators)
{
    EXPECT_EQ(renderValue("not a and b"), "((not a) and b)");
    EXPECT_EQ(renderValue("abs a + 1"), "((abs a) + 1)");
    EXPECT_EQ(renderValue("2 ** n"), "(2 ** n)");
    EXPECT_TRUE(rejects("a ** b ** c"));
    EXPECT_TRUE(rejects("not not a"));
}

TEST(ParserExpressions, UnaryReductionOperators)
{
    EXPECT_EQ(renderValue("and v"), "(and v)");
    EXPECT_EQ(renderValue("xor v or b"), "((xor v) or b)");
    EXPECT_EQ(renderValue("a and nand v"), "(a and (nand v))");
}

TEST(ParserExpressions, ConditionOperator)
{
    EXPECT_EQ(renderValue("?? en"), "(?? en)");
    EXPECT_TRUE(rejects("a and ?? b"));
}

TEST(ParserExpressions, MatchingRelationalOperators)
{
    for (const char* op : { "?=", "?/=", "?<", "?<=", "?>", "?>=" })
        EXPECT_EQ(renderValue(std::string("a ") + op + " b"), std::string("(a ") + op + " b)") << op;
}

TEST(ParserExpressions, NonAssociativeLevels)
{
    EXPECT_TRUE(rejects("a = b = c"));
    EXPECT_TRUE(rejects("a sll 1 sll 2"));
}

// ===========================================================================
// 2. LOGICAL OPERATORS
// ===========================================================================

TEST(ParserExpressions, SameLogicalOperatorChains)
{
    EXPECT_EQ(renderValue("a and b and c"), "((a and b) and c)");
    EXPECT_EQ(renderValue("a xnor b xnor c"), "((a xnor b) xnor c)");
}

TEST(ParserExpressions, MixedLogicalOperatorsNeedParentheses)
{
    EXPECT_NE(errorOf("a and b or c").find("Mixing 'and' and 'or' requires parentheses"), std::string::npos);
    EXPECT_EQ(renderValue("(a and b) or c"), "((a and b) or c)");
}

TEST(ParserExpressions, NandAndNorDoNotChain)
{
    EXPECT_EQ(renderValue("a nand b"), "(a nand b)");
    EXPECT_NE(errorOf("a nand b nand c").find("cannot be chained"), std::string::npos);
    EXPECT_TRUE(rejects("a nor b nor c"));
}

// ===========================================================================
// 3. NAMES
// ===========================================================================

TEST(ParserExpressions, CallsIndexesAndSlices)
{
    EXPECT_EQ(renderValue("f(a, b)"), "f[a, b]");
    EXPECT_EQ(renderValue("v(3)"), "v[3]");
    EXPECT_EQ(renderValue("v(7 downto 4)"), "v[(7 downto 4)]");
    EXPECT_EQ(renderValue("m(1)(2)"), "m[1][2]");
    EXPECT_EQ(renderValue("f(x => 1, y => open)"), "f[x => 1, y => open]");
    EXPECT_EQ(renderValue("v(t range 0 to 1)"), "v[t range (0 to 1)]");
}

TEST(ParserExpressions, SelectedNames)
{
    EXPECT_EQ(renderValue("rec.field.sub"), "rec.field.sub");
    EXPECT_EQ(renderValue("ptr.all"), "ptr.all");
    EXPECT_EQ(renderValue("arr(1).x"), "arr[1].x");
}

TEST(ParserExpressions, Attributes)
{
    EXPECT_EQ(renderValue("clk'event and clk = '1'"), "(clk'event and (clk = '1'))");
    EXPECT_EQ(renderValue("v'range"), "v'range");
    EXPECT_EQ(renderValue("v'length - 1"), "(v'length - 1)");
    EXPECT_EQ(renderValue("t'image(x)"), "t'image[x]");
    EXPECT_EQ(renderValue("v(1)'length"), "v[1]'length");
    EXPECT_EQ(renderValue("t'subtype'high"), "t'subtype'high");
}

TEST(ParserExpressions, AttributeWithSignature)
{
    ASTRoot root = parseSource(inArchitecture("", "y <= f[bit return bit]'path_name;"));
    auto* attribute = as<AttributeExpr>(as<SignalAssignment>(architectureBody(root).at(0).get())->value.get());
    EXPECT_EQ(attribute->attributeName, "path_name");
    ASSERT_NE(attribute->signature, nullptr);
    EXPECT_EQ(attribute->signature->parameters.size(), 1u);
}

TEST(ParserExpressions, QualifiedExpressions)
{
    EXPECT_EQ(renderValue("unsigned'(\"0101\")"), "unsigned'(\"0101\")");
    EXPECT_EQ(renderValue("t'(others => '0')"), "t'({others => '0'})");
    EXPECT_EQ(renderValue("t'(a) + 1"), "(t'(a) + 1)");
    EXPECT_TRUE(rejects("f(x)'(a)")) << "only a type mark can be qualified";
}

TEST(ParserExpressions, OperatorSymbolCall)
{
    EXPECT_EQ(renderValue("\"AND\"(a, b)"), "\"and\"[a, b]");
}

TEST(ParserExpressions, AnOperatorSymbolCanBeSelectedFromAPrefix)
{
    ASTRoot root = parseSource(inArchitecture("", "y <= work.pkg.\"AND\"(a, b);"));
    auto* call = as<FunctionCallExpr>(as<SignalAssignment>(architectureBody(root).at(0).get())->value.get());
    auto* selected = as<FieldAccessExpr>(call->callee.get());
    EXPECT_EQ(selected->fieldName, "\"and\"");
    EXPECT_EQ(call->arguments.size(), 2u);

    EXPECT_EQ(renderValue("p.'a'"), "p.'a'");
    EXPECT_EQ(renderValue("p.all"), "p.all");
    EXPECT_TRUE(rejects("p.(a)")) << "only a name, a character, an operator symbol or `all` can be selected";
    EXPECT_TRUE(rejects("p.")) << "a selection needs something after the dot";
}

TEST(ParserExpressions, ExternalNames)
{
    ASTRoot root = parseSource(inArchitecture("", "y <= << signal .tb.dut.count : unsigned(3 downto 0) >>;"));
    auto* external = as<ExternalNameExpr>(as<SignalAssignment>(architectureBody(root).at(0).get())->value.get());
    EXPECT_EQ(external->objectClass, ExternalObjectClass::Signal);
    EXPECT_EQ(external->path, ".tb.dut.count");
    EXPECT_EQ(render(external->subtype.get()), "unsigned((3 downto 0))");

    EXPECT_NO_THROW(parseSource(inArchitecture("", "y <= <<constant @lib.pkg.c : integer>>;")));
    EXPECT_NO_THROW(parseSource(inArchitecture("", "y <= <<variable ^.^.v : bit>>;")));
    EXPECT_TRUE(rejects("<<signal : bit>>"));
    EXPECT_TRUE(rejects("<<wire .a : bit>>"));
    EXPECT_TRUE(rejects("<<signal .a : bit"));
}

TEST(ParserExpressions, ExternalNamesAreNamesEverywhere)
{
    // Target of an assignment, sensitivity list, wait list and prefix of an attribute or an index.
    ASTRoot root = parseSource(inArchitecture("", "<<signal .tb.x : bit>> <= '1'; "
                                                   "process (<<signal .tb.y : bit>>) begin wait on <<signal .tb.z : bit>>; end process; "
                                                   "w <= <<signal .tb.v : bit_vector(3 downto 0)>>(2) and <<signal .tb.y : bit>>'event;"));
    const auto& body = architectureBody(root);
    ASSERT_EQ(body.size(), 3u);
    EXPECT_NE(dynamic_cast<const ExternalNameExpr*>(as<SignalAssignment>(body[0].get())->target.get()), nullptr);

    auto* process = as<ProcessStatement>(body[1].get());
    ASSERT_EQ(process->sensitivityList.size(), 1u);
    EXPECT_NE(dynamic_cast<const ExternalNameExpr*>(process->sensitivityList[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<const ExternalNameExpr*>(as<WaitStatement>(process->body.at(0).get())->onSignals.at(0).get()), nullptr);

    auto* value = as<BinaryOpExpr>(as<SignalAssignment>(body[2].get())->value.get());
    auto* indexed = as<FunctionCallExpr>(value->left.get());
    EXPECT_NE(dynamic_cast<const ExternalNameExpr*>(indexed->callee.get()), nullptr);
    EXPECT_NE(dynamic_cast<const AttributeExpr*>(value->right.get()), nullptr);
}

TEST(ParserExpressions, AllocatorsAndNullAreNotSupported)
{
    EXPECT_TRUE(rejects("new node"));
    EXPECT_TRUE(rejects("new t'(1)"));
    EXPECT_TRUE(rejects("null"));
}

// ===========================================================================
// 4. AGGREGATES
// ===========================================================================

TEST(ParserExpressions, ParenthesesAreNotAggregates)
{
    EXPECT_EQ(renderValue("(a)"), "a");
    EXPECT_EQ(renderValue("((a + b)) * c"), "((a + b) * c)");
}

TEST(ParserExpressions, PositionalAndNamedAggregates)
{
    EXPECT_EQ(renderValue("(a, b, c)"), "{a, b, c}");
    EXPECT_EQ(renderValue("(others => '0')"), "{others => '0'}");
    EXPECT_EQ(renderValue("(0 => '1', others => '0')"), "{0 => '1', others => '0'}");
    EXPECT_EQ(renderValue("(1 | 3 => a, 4 to 7 => b)"), "{1 | 3 => a, (4 to 7) => b}");
    EXPECT_EQ(renderValue("(x => 1, y => 2)"), "{x => 1, y => 2}");
    EXPECT_EQ(renderValue("(7 downto 4 => '1')"), "{(7 downto 4) => '1'}");
}

TEST(ParserExpressions, SingleNamedElementStaysAnAggregate)
{
    EXPECT_EQ(renderValue("(0 => '1')"), "{0 => '1'}");
}

TEST(ParserExpressions, AggregateGrammar)
{
    EXPECT_TRUE(rejects("()"));
    EXPECT_TRUE(rejects("(a,)"));
    EXPECT_TRUE(rejects("(others)"));
    EXPECT_TRUE(rejects("(a => )"));
    EXPECT_TRUE(rejects("(a b)"));
}

// ===========================================================================
// 5. ROBUSTNESS
// ===========================================================================

TEST(ParserExpressions, DeepParenthesesFailCleanly)
{
    const std::string deep = std::string(10000, '(') + "a" + std::string(10000, ')');
    std::string message = errorOf(deep);
    EXPECT_NE(message.find("Nesting is too deep"), std::string::npos) << message;
}

TEST(ParserExpressions, ModerateNestingIsFine)
{
    const std::string nested = std::string(50, '(') + "a" + std::string(50, ')');
    EXPECT_EQ(renderValue(nested), "a");
}

TEST(ParserExpressions, MissingOperands)
{
    for (const char* expression : { "a +", "* b", "a and", "(a", "a)", "f(", "v'", "a.", "a = ", "?? " })
        EXPECT_TRUE(rejects(expression)) << expression;
}

TEST(ParserExpressions, ErrorsPointAtTheOffendingToken)
{
    SyntaxError error = syntaxError(inArchitecture("", "y <= a + ;"));
    ASSERT_TRUE(error.thrown);
    EXPECT_EQ(error.line, 5u);
    EXPECT_EQ(error.column, 10u);
    EXPECT_NE(error.message.find("Expected an expression, but found ';'"), std::string::npos) << error.message;
}
