#include "parser_impl.h"

namespace Pulse::Parser
{
    // ---- Assertions -----------------------------------------------------------------------------

    /// `assert condition [report message] [severity level] ;` (sequential and concurrent).
    StatementPtr Parser::parseAssert()
    {
        auto statement = node<AssertStatement>(tokens.expect("assert"));
        statement->condition = parseExpression();
        statement->message = parseOptionalClause("report");
        statement->severity = parseOptionalClause("severity");
        tokens.expect(";");
        return statement;
    }

    // ---- Assignments and procedure calls --------------------------------------------------------

    /// `target <= value ;`, `target := value ;` (sequential only) or `procedure_call ;`.
    StatementPtr Parser::parseAssignmentOrCall(bool sequential)
    {
        ExpressionPtr target = parseTarget();
        if (tokens.at("<="))
            return finishSignalAssignment(std::move(target));
        if (sequential && tokens.at(":="))
            return finishVariableAssignment(std::move(target));
        if (tokens.at(";"))
            return finishProcedureCall(std::move(target));
        tokens.expected(sequential ? "'<=', ':=' or ';'" : "'<=' or ';'");
    }

    /// A name, or an aggregate of names: `(a, b) <= v;`.
    ExpressionPtr Parser::parseTarget()
    {
        if (tokens.at("("))
            return parseAggregateOrParenthesized();
        return parseName();
    }

    StatementPtr Parser::finishSignalAssignment(ExpressionPtr target)
    {
        auto statement = node<SignalAssignment>(tokens.expect("<="));
        statement->target = std::move(target);
        statement->value = parseConditionalValue();
        tokens.expect(";");
        return statement;
    }

    StatementPtr Parser::finishVariableAssignment(ExpressionPtr target)
    {
        auto statement = node<VariableAssignment>(tokens.expect(":="));
        statement->target = std::move(target);
        statement->value = parseConditionalValue();
        tokens.expect(";");
        return statement;
    }

    StatementPtr Parser::finishProcedureCall(ExpressionPtr call)
    {
        if (dynamic_cast<const AggregateExpr*>(call.get()))
            tokens.expected("'<='");

        auto statement = node<ProcedureCallStatement>(tokens.expect(";"));
        statement->call = std::move(call);
        return statement;
    }

    // ---- Selected assignments -------------------------------------------------------------------

    /// `with selector select[?] target <= value when choices {, value when choices} ;`
    std::unique_ptr<WithClause> Parser::parseSelectedAssignment()
    {
        auto statement = node<WithClause>(tokens.expect("with"));
        statement->selector = parseExpression();
        tokens.expect("select");
        statement->matching = tokens.accept("?") != nullptr;
        statement->target = parseTarget();
        tokens.expect("<=");
        parseSeparated(",", [&] { statement->choices.push_back(parseSelectedChoice()); });
        tokens.expect(";");
        return statement;
    }

    /// `value when choices`
    std::unique_ptr<SelectedChoice> Parser::parseSelectedChoice()
    {
        auto choice = node<SelectedChoice>(tokens.peek());
        choice->value = parseValue();
        tokens.expect("when");
        choice->choices = parseChoices();
        return choice;
    }

    // ---- Values ---------------------------------------------------------------------------------

    /// `value [when condition [else value when condition ...] [else value]]`, built as a chain of
    /// WhenElseExprs through `falseValue`. Iterative, so a long chain cannot exhaust the stack while parsing.
    ExpressionPtr Parser::parseConditionalValue()
    {
        ExpressionPtr first = parseValue();
        if (!tokens.at("when"))
            return first;

        std::unique_ptr<WhenElseExpr> root = startWhenElse(std::move(first));
        WhenElseExpr* last = root.get();
        ChainScope chain(*this);
        while (tokens.accept("else"))
        {
            chain.extend();
            ExpressionPtr value = parseValue();
            if (!tokens.at("when"))
            {
                last->falseValue = std::move(value);
                break;
            }
            std::unique_ptr<WhenElseExpr> branch = startWhenElse(std::move(value));
            WhenElseExpr* next = branch.get();
            last->falseValue = std::move(branch);
            last = next;
        }
        return root;
    }

    /// `when condition` after an already parsed value.
    std::unique_ptr<WhenElseExpr> Parser::startWhenElse(ExpressionPtr value)
    {
        auto branch = std::make_unique<WhenElseExpr>();
        branch->source = value->source;
        branch->trueValue = std::move(value);
        tokens.expect("when");
        branch->condition = parseExpression();
        return branch;
    }

    ExpressionPtr Parser::parseValue()
    {
        if (const Token* unaffected = tokens.accept("unaffected"))
            return node<UnaffectedExpr>(unaffected);
        return parseExpression();
    }

} // namespace Pulse::Parser
