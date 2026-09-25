// sequential.test.cc — the statements of a process or subprogram body.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    /// Parses `statements` as a process body; the statements are processBody(root).
    ASTRoot process(const std::string& statements, const std::string& declarations = "")
    {
        return parseSource(inProcess(statements, declarations));
    }

    bool rejects(const std::string& statements)
    {
        return syntaxError(inProcess(statements)).thrown;
    }
}

// ===========================================================================
// 1. ASSIGNMENTS
// ===========================================================================

TEST(ParserSequential, SignalAndVariableAssignments)
{
    ASTRoot root = process("s <= a; v := b + 1; r.x := '1';");
    const auto& body = processBody(root);
    ASSERT_EQ(body.size(), 3u);
    EXPECT_EQ(render(as<SignalAssignment>(body[0].get())->value.get()), "a");
    EXPECT_EQ(render(as<VariableAssignment>(body[1].get())->value.get()), "(b + 1)");
    EXPECT_EQ(render(as<VariableAssignment>(body[2].get())->target.get()), "r.x");
}

TEST(ParserSequential, ConditionalAssignmentsInsideAProcess)
{
    ASTRoot root = process("s <= a when c else b; v := 1 when c else 2;");
    const auto& body = processBody(root);
    EXPECT_EQ(render(as<SignalAssignment>(body[0].get())->value.get()), "(a when c else b)");
    EXPECT_EQ(render(as<VariableAssignment>(body[1].get())->value.get()), "(1 when c else 2)");
}

TEST(ParserSequential, SelectedAssignmentInsideAProcess)
{
    ASTRoot root = process("with s select y <= a when '0', b when others;");
    EXPECT_EQ(as<WithClause>(processBody(root).at(0).get())->choices.size(), 2u);
}

TEST(ParserSequential, AssignmentGrammar)
{
    EXPECT_TRUE(rejects("s <= ;"));
    EXPECT_TRUE(rejects("s <= a"));
    EXPECT_TRUE(rejects("s = a;"));
    EXPECT_TRUE(rejects("(a, b);")) << "an aggregate is not a procedure call";
    EXPECT_TRUE(rejects("'1' <= a;"));
    EXPECT_TRUE(rejects("s <= force a;"));
    EXPECT_TRUE(rejects("s <= release;"));
}

// ===========================================================================
// 2. IF
// ===========================================================================

TEST(ParserSequential, IfElsifElse)
{
    ASTRoot root = process(
        "if a = '1' then x := 1; y := 2;\n"
        "elsif b then x := 3;\n"
        "elsif c then null;\n"
        "else x := 4;\n"
        "end if;");
    auto* statement = as<IfStatement>(processBody(root).at(0).get());
    ASSERT_EQ(statement->branches.size(), 3u);
    EXPECT_EQ(render(statement->branches[0]->condition.get()), "(a = '1')");
    EXPECT_EQ(statement->branches[0]->body.size(), 2u);
    EXPECT_EQ(render(statement->branches[2]->condition.get()), "c");
    EXPECT_EQ(statement->elseBody.size(), 1u);
    EXPECT_EQ(statement->branches[1]->source.line, 9u) << "the elsif keyword, on the second statement line";
}

TEST(ParserSequential, IfWithEmptyBranchesAndLabel)
{
    ASTRoot root = process("chk : if a then else end if chk;");
    auto* statement = as<IfStatement>(processBody(root).at(0).get());
    EXPECT_EQ(statement->label, "chk");
    EXPECT_TRUE(statement->branches.at(0)->body.empty());
    EXPECT_TRUE(statement->elseBody.empty());
}

TEST(ParserSequential, IfGrammar)
{
    EXPECT_TRUE(rejects("if a then null; end;"));
    EXPECT_TRUE(rejects("if a null; end if;"));
    EXPECT_TRUE(rejects("if a then else null; elsif b then end if;"));
    EXPECT_TRUE(rejects("if a then end if other;"));
    EXPECT_TRUE(rejects("l : if a then end if other;"));
}

// ===========================================================================
// 3. CASE
// ===========================================================================

TEST(ParserSequential, Case)
{
    ASTRoot root = process(
        "case state is\n"
        "  when idle | stop => y := 0;\n"
        "  when 1 to 3 => null;\n"
        "  when others => y := 1; z := 2;\n"
        "end case;");
    auto* statement = as<CaseStatement>(processBody(root).at(0).get());
    EXPECT_FALSE(statement->matching);
    EXPECT_EQ(render(statement->selector.get()), "state");
    ASSERT_EQ(statement->alternatives.size(), 3u);
    EXPECT_EQ(render(statement->alternatives[0]->choices.get()), "idle | stop");
    EXPECT_EQ(render(statement->alternatives[1]->choices.get()), "(1 to 3)");
    EXPECT_EQ(render(statement->alternatives[2]->choices.get()), "others");
    EXPECT_EQ(statement->alternatives[2]->body.size(), 2u);
}

TEST(ParserSequential, MatchingCase)
{
    ASTRoot root = process("case? v is when \"1-\" => null; when others => null; end case?;");
    EXPECT_TRUE(as<CaseStatement>(processBody(root).at(0).get())->matching);
}

TEST(ParserSequential, CaseGrammar)
{
    EXPECT_TRUE(rejects("case v is end case;")) << "at least one alternative";
    EXPECT_TRUE(rejects("case? v is when others => null; end case;")) << "case? closes with end case?";
    EXPECT_TRUE(rejects("case v is when others => null; end case?;"));
    EXPECT_TRUE(rejects("case v is when => null; end case;"));
    EXPECT_TRUE(rejects("case v is when 1 null; end case;"));
}

// ===========================================================================
// 4. LOOPS
// ===========================================================================

TEST(ParserSequential, ForLoopRanges)
{
    ASTRoot root = process(
        "for i in 0 to 7 loop null; end loop;\n"
        "for j in v'range loop end loop;\n"
        "for k in state loop end loop;\n"
        "for m in natural range 0 to 3 loop end loop;");
    const auto& body = processBody(root);
    ASSERT_EQ(body.size(), 4u);
    EXPECT_EQ(as<ForLoopStatement>(body[0].get())->parameter, "i");
    EXPECT_EQ(render(as<ForLoopStatement>(body[0].get())->range.get()), "(0 to 7)");
    EXPECT_EQ(render(as<ForLoopStatement>(body[1].get())->range.get()), "v'range");
    EXPECT_EQ(render(as<ForLoopStatement>(body[2].get())->range.get()), "state");
    EXPECT_EQ(render(as<ForLoopStatement>(body[3].get())->range.get()), "natural range (0 to 3)");
}

TEST(ParserSequential, WhileAndPlainLoopsWithLabels)
{
    ASTRoot root = process("outer : while n > 0 loop inner : loop exit outer when done; next; end loop inner; end loop outer;");
    auto* outer = as<WhileLoopStatement>(processBody(root).at(0).get());
    EXPECT_EQ(outer->label, "outer");
    EXPECT_EQ(render(outer->condition.get()), "(n > 0)");
    auto* inner = as<LoopStatement>(outer->body.at(0).get());
    EXPECT_EQ(inner->label, "inner");
    ASSERT_EQ(inner->body.size(), 2u);

    auto* exit = as<ExitStatement>(inner->body[0].get());
    EXPECT_EQ(exit->loopLabel, "outer");
    EXPECT_EQ(render(exit->condition.get()), "done");

    auto* next = as<NextStatement>(inner->body[1].get());
    EXPECT_TRUE(next->loopLabel.empty());
    EXPECT_EQ(next->condition, nullptr);
}

TEST(ParserSequential, LoopGrammar)
{
    EXPECT_TRUE(rejects("for i in 0 to 3 null; end loop;"));
    EXPECT_TRUE(rejects("for 0 to 3 loop end loop;"));
    EXPECT_TRUE(rejects("loop end;"));
    EXPECT_TRUE(rejects("l : loop end loop m;"));
    EXPECT_TRUE(rejects("exit when;"));
    EXPECT_TRUE(rejects("next l l;"));
}

// ===========================================================================
// 5. WAIT, ASSERT, REPORT, NULL
// ===========================================================================

TEST(ParserSequential, WaitForms)
{
    ASTRoot root = process("wait; wait on a, b; wait until clk = '1'; wait for 10 ns; wait on a until b for 1 us;");
    const auto& body = processBody(root);
    ASSERT_EQ(body.size(), 5u);

    auto* forever = as<WaitStatement>(body[0].get());
    EXPECT_TRUE(forever->onSignals.empty());
    EXPECT_EQ(forever->until, nullptr);
    EXPECT_EQ(forever->timeout, nullptr);

    EXPECT_EQ(as<WaitStatement>(body[1].get())->onSignals.size(), 2u);
    EXPECT_EQ(render(as<WaitStatement>(body[2].get())->until.get()), "(clk = '1')");
    EXPECT_EQ(render(as<WaitStatement>(body[3].get())->timeout.get()), "10 ns");

    auto* all = as<WaitStatement>(body[4].get());
    EXPECT_EQ(all->onSignals.size(), 1u);
    EXPECT_NE(all->until, nullptr);
    EXPECT_NE(all->timeout, nullptr);
}

TEST(ParserSequential, WaitClausesKeepTheirOrder)
{
    EXPECT_TRUE(rejects("wait for 1 ns until a;"));
    EXPECT_TRUE(rejects("wait until a on b;"));
}

TEST(ParserSequential, AssertAndReport)
{
    ASTRoot root = process("assert ok; assert ok report \"x=\" & to_string(x) severity error; report \"done\"; report m severity note;");
    const auto& body = processBody(root);
    ASSERT_EQ(body.size(), 4u);

    auto* bare = as<AssertStatement>(body[0].get());
    EXPECT_EQ(bare->message, nullptr);
    EXPECT_EQ(bare->severity, nullptr);

    auto* full = as<AssertStatement>(body[1].get());
    EXPECT_EQ(render(full->message.get()), "(\"x=\" & to_string[x])");
    EXPECT_EQ(render(full->severity.get()), "error");

    EXPECT_EQ(render(as<ReportStatement>(body[2].get())->message.get()), "\"done\"");
    EXPECT_EQ(render(as<ReportStatement>(body[3].get())->severity.get()), "note");
}

TEST(ParserSequential, NullStatement)
{
    ASTRoot root = process("lbl : null;");
    EXPECT_EQ(as<NullStatement>(processBody(root).at(0).get())->label, "lbl");
}

TEST(ParserSequential, ConcurrentOnlyStatementsAreRejected)
{
    EXPECT_TRUE(rejects("process begin end process;"));
    EXPECT_TRUE(rejects("u : comp port map (a);"));
}

TEST(ParserSequential, DeeplyNestedStatementsFailCleanly)
{
    std::string statements;
    for (int i = 0; i < 5000; ++i)
        statements += "if a then ";
    for (int i = 0; i < 5000; ++i)
        statements += "end if; ";

    SyntaxError error = syntaxError(inProcess(statements));
    ASSERT_TRUE(error.thrown);
    EXPECT_NE(error.message.find("Nesting is too deep"), std::string::npos) << error.message;
}

TEST(ParserSequential, ReasonableNestingIsFine)
{
    std::string statements;
    for (int i = 0; i < 40; ++i)
        statements += "if a then ";
    for (int i = 0; i < 40; ++i)
        statements += "end if; ";
    EXPECT_NO_THROW(parseSource(inProcess(statements)));
}
