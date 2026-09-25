// concurrent.test.cc — the statements of an architecture body.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    /// Parses `statements` as an architecture body and returns the root (the body is architectureBody(root)).
    ASTRoot body(const std::string& statements)
    {
        return parseSource(inArchitecture("", statements));
    }
}

// ===========================================================================
// 1. SIGNAL ASSIGNMENTS
// ===========================================================================

TEST(ParserConcurrent, SimpleAssignment)
{
    ASTRoot root = body("y <= a and b;");
    auto* assignment = as<SignalAssignment>(architectureBody(root).at(0).get());
    EXPECT_EQ(render(assignment->target.get()), "y");
    EXPECT_EQ(render(assignment->value.get()), "(a and b)");
    EXPECT_TRUE(assignment->label.empty());
}

TEST(ParserConcurrent, LabeledAssignmentWithIndexedTarget)
{
    ASTRoot root = body("drive : y(3 downto 0) <= x\"A\";");
    auto* assignment = as<SignalAssignment>(architectureBody(root).at(0).get());
    EXPECT_EQ(assignment->label, "drive");
    EXPECT_EQ(render(assignment->target.get()), "y[(3 downto 0)]");
    EXPECT_EQ(render(assignment->value.get()), "\"1010\"");
    EXPECT_EQ(assignment->source.column, 1u) << "a labeled statement starts at its label";
}

TEST(ParserConcurrent, AggregateTarget)
{
    ASTRoot root = body("(carry, sum) <= a + b;");
    EXPECT_EQ(render(as<SignalAssignment>(architectureBody(root).at(0).get())->target.get()), "{carry, sum}");
}

TEST(ParserConcurrent, ConditionalAssignmentNestsThroughElse)
{
    ASTRoot root = body("y <= a when s = \"00\" else b when s = \"01\" else c;");
    auto* assignment = as<SignalAssignment>(architectureBody(root).at(0).get());
    EXPECT_EQ(render(assignment->value.get()), "(a when (s = \"00\") else (b when (s = \"01\") else c))");
}

TEST(ParserConcurrent, ConditionalAssignmentWithoutFinalElse)
{
    ASTRoot root = body("y <= a when en = '1';");
    auto* value = as<WhenElseExpr>(as<SignalAssignment>(architectureBody(root).at(0).get())->value.get());
    EXPECT_EQ(value->falseValue, nullptr);
}

TEST(ParserConcurrent, Unaffected)
{
    ASTRoot root = body("y <= a when en = '1' else unaffected;");
    EXPECT_EQ(render(as<SignalAssignment>(architectureBody(root).at(0).get())->value.get()),
              "(a when (en = '1') else unaffected)");
}

TEST(ParserConcurrent, LongConditionalChainsDoNotRecurse)
{
    std::string chain = "y <= ";
    for (int i = 0; i < 2000; ++i)
        chain += "v" + std::to_string(i) + " when s = " + std::to_string(i) + " else ";
    chain += "z;";

    ASTRoot root = body(chain);
    const Expression* value = as<SignalAssignment>(architectureBody(root).at(0).get())->value.get();
    int depth = 0;
    while (auto* whenElse = dynamic_cast<const WhenElseExpr*>(value))
    {
        value = whenElse->falseValue.get();
        ++depth;
    }
    EXPECT_EQ(depth, 2000);
    EXPECT_EQ(render(value), "z");
}

TEST(ParserConcurrent, DelaysAreNotSupported)
{
    EXPECT_TRUE(syntaxError(inArchitecture("", "y <= a after 1 ns;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "y <= transport a;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "y <= a, b;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "y <= guarded a;")).thrown);
}

TEST(ParserConcurrent, VariableAssignmentIsSequentialOnly)
{
    SyntaxError error = syntaxError(inArchitecture("", "v := 1;"));
    ASSERT_TRUE(error.thrown);
    EXPECT_NE(error.message.find("Expected '<=' or ';', but found ':='"), std::string::npos) << error.message;
}

// ===========================================================================
// 2. SELECTED ASSIGNMENTS
// ===========================================================================

TEST(ParserConcurrent, SelectedAssignment)
{
    ASTRoot root = body("with sel select y <= a when \"00\", b when \"01\" | \"10\", c when others;");
    auto* with = as<WithClause>(architectureBody(root).at(0).get());
    EXPECT_FALSE(with->matching);
    EXPECT_EQ(render(with->selector.get()), "sel");
    EXPECT_EQ(render(with->target.get()), "y");
    ASSERT_EQ(with->choices.size(), 3u);
    EXPECT_EQ(render(with->choices[1]->value.get()), "b");
    EXPECT_EQ(render(with->choices[1]->choices.get()), "\"01\" | \"10\"");
    EXPECT_EQ(render(with->choices[2]->choices.get()), "others");
}

TEST(ParserConcurrent, MatchingSelectedAssignment)
{
    ASTRoot root = body("with sel select? y <= a when \"1-\", b when others;");
    EXPECT_TRUE(as<WithClause>(architectureBody(root).at(0).get())->matching);
}

TEST(ParserConcurrent, SelectedAssignmentGrammar)
{
    EXPECT_TRUE(syntaxError(inArchitecture("", "with s select y <= a;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "with s select y <= a when 0 b when 1;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "with s y <= a when 0;")).thrown);
}

// ===========================================================================
// 3. COMPONENT INSTANTIATIONS
// ===========================================================================

TEST(ParserConcurrent, InstantiationWithNamedPorts)
{
    ASTRoot root = body("u1 : adder port map (a => x, b => open, s => y(0));");
    auto* instance = as<ComponentInstantiation>(architectureBody(root).at(0).get());
    EXPECT_EQ(instance->label, "u1");
    EXPECT_EQ(instance->componentName, "adder");
    ASSERT_EQ(instance->portMap.size(), 3u);
    EXPECT_EQ(render(instance->portMap[1].get()), "b => open");
    EXPECT_EQ(render(instance->portMap[2].get()), "s => y[0]");
}

TEST(ParserConcurrent, InstantiationWithComponentKeywordAndGenerics)
{
    ASTRoot root = body("u2 : component reg generic map (8, init => 0) port map (clk, d, q);");
    auto* instance = as<ComponentInstantiation>(architectureBody(root).at(0).get());
    EXPECT_EQ(instance->componentName, "reg");
    ASSERT_EQ(instance->genericMap.size(), 2u);
    EXPECT_EQ(render(instance->genericMap[0].get()), "8");
    EXPECT_EQ(render(instance->genericMap[1].get()), "init => 0");
    EXPECT_EQ(instance->portMap.size(), 3u);
}

TEST(ParserConcurrent, LabeledNameAloneIsAnInstantiation)
{
    ASTRoot root = body("u3 : empty_block;");
    auto* instance = as<ComponentInstantiation>(architectureBody(root).at(0).get());
    EXPECT_EQ(instance->componentName, "empty_block");
    EXPECT_TRUE(instance->portMap.empty());
}

TEST(ParserConcurrent, ConversionInAFormal)
{
    ASTRoot root = body("u : c port map (to_integer(a) => b);");
    auto* instance = as<ComponentInstantiation>(architectureBody(root).at(0).get());
    EXPECT_EQ(render(instance->portMap.at(0).get()), "to_integer[a] => b");
}

TEST(ParserConcurrent, InstantiationGrammar)
{
    EXPECT_TRUE(syntaxError(inArchitecture("", "adder port map (a);")).thrown) << "an instance needs a label";
    EXPECT_TRUE(syntaxError(inArchitecture("", "u : adder port map ();")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "u : adder port (a);")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "u : entity work.adder port map (a);")).thrown) << "entity instantiation";
}

// ===========================================================================
// 4. PROCESSES
// ===========================================================================

TEST(ParserConcurrent, ProcessWithSensitivityList)
{
    ASTRoot root = body(
        "reg : process (clk, rst) is\n"
        "  variable n : integer;\n"
        "begin\n"
        "  if rst = '1' then q <= '0'; end if;\n"
        "end process reg;");
    auto* process = as<ProcessStatement>(architectureBody(root).at(0).get());
    EXPECT_EQ(process->label, "reg");
    EXPECT_FALSE(process->sensitivityAll);
    ASSERT_EQ(process->sensitivityList.size(), 2u);
    EXPECT_EQ(render(process->sensitivityList[1].get()), "rst");
    EXPECT_EQ(process->declarations.size(), 1u);
    EXPECT_EQ(process->body.size(), 1u);
}

TEST(ParserConcurrent, ProcessAll)
{
    ASTRoot root = body("process (all) begin y <= a; end process;");
    auto* process = as<ProcessStatement>(architectureBody(root).at(0).get());
    EXPECT_TRUE(process->sensitivityAll);
    EXPECT_TRUE(process->sensitivityList.empty());
}

TEST(ParserConcurrent, ProcessWithoutSensitivityList)
{
    ASTRoot root = body("process begin wait; end process;");
    auto* process = as<ProcessStatement>(architectureBody(root).at(0).get());
    EXPECT_TRUE(process->sensitivityList.empty());
    EXPECT_FALSE(process->sensitivityAll);
}

TEST(ParserConcurrent, ProcessClosing)
{
    EXPECT_TRUE(syntaxError(inArchitecture("", "p : process begin wait; end process q;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "process begin wait; end process p;")).thrown) << "an unlabeled process has no closing name";
    EXPECT_TRUE(syntaxError(inArchitecture("", "process begin wait; end;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "postponed process begin wait; end postponed process;")).thrown);
}

TEST(ParserConcurrent, ProcessSensitivityGrammar)
{
    EXPECT_TRUE(syntaxError(inArchitecture("", "process () begin wait; end process;")).thrown);
    EXPECT_TRUE(syntaxError(inArchitecture("", "process (all, clk) begin wait; end process;")).thrown);
}

// ===========================================================================
// 5. OTHER STATEMENTS
// ===========================================================================

TEST(ParserConcurrent, ConcurrentAssertion)
{
    ASTRoot root = body("check : assert width > 0 report \"bad\" severity failure;");
    auto* assertion = as<AssertStatement>(architectureBody(root).at(0).get());
    EXPECT_EQ(assertion->label, "check");
    EXPECT_EQ(render(assertion->message.get()), "\"bad\"");
    EXPECT_EQ(render(assertion->severity.get()), "failure");
}

TEST(ParserConcurrent, SequentialStatementsAreRejected)
{
    for (const char* statement : { "if a then end if;", "wait;", "null;", "for i in 0 to 1 loop end loop;", "report \"x\";" })
        EXPECT_TRUE(syntaxError(inArchitecture("", statement)).thrown) << statement;
}

TEST(ParserConcurrent, UnsupportedConcurrentStatements)
{
    for (const char* statement : { "g : for i in 0 to 3 generate end generate;", "g : if c generate end generate;",
                                   "b : block begin end block;" })
        EXPECT_TRUE(syntaxError(inArchitecture("", statement)).thrown) << statement;
}
