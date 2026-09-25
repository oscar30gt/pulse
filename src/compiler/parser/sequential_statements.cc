#include "parser_impl.h"

namespace Pulse::Parser
{
    /// Statements up to (not including) one of the `stops` keywords.
    std::vector<StatementPtr> Parser::parseSequentialStatements(std::initializer_list<std::string_view> stops)
    {
        std::vector<StatementPtr> statements;
        while (!tokens.atAny(stops))
            statements.push_back(parseSequentialStatement());
        return statements;
    }

    /// `[label :] statement`
    StatementPtr Parser::parseSequentialStatement()
    {
        NestingGuard guard(*this);
        const Token* start = tokens.peek();
        std::string label = parseOptionalLabel();
        StatementPtr statement = parseUnlabeledSequentialStatement(label);
        statement->source = locationOf(start);
        statement->label = std::move(label);
        return statement;
    }

    StatementPtr Parser::parseUnlabeledSequentialStatement(const std::string& label)
    {
        if (tokens.at("if"))     return parseIf(label);
        if (tokens.at("case"))   return parseCase(label);
        if (tokens.at("for"))    return parseForLoop(label);
        if (tokens.at("while"))  return parseWhileLoop(label);
        if (tokens.at("loop"))   return parsePlainLoop(label);
        if (tokens.at("exit"))   return parseExit();
        if (tokens.at("next"))   return parseNext();
        if (tokens.at("null"))   return parseNull();
        if (tokens.at("wait"))   return parseWait();
        if (tokens.at("assert")) return parseAssert();
        if (tokens.at("report")) return parseReport();
        if (tokens.at("return")) return parseReturn();
        if (tokens.at("with"))   return parseSelectedAssignment();
        return parseAssignmentOrCall(true);
    }

    // ---- if -------------------------------------------------------------------------------------

    /// `if c then ... {elsif c then ...} [else ...] end if [label] ;`
    std::unique_ptr<IfStatement> Parser::parseIf(const std::string& label)
    {
        auto statement = node<IfStatement>(tokens.peek());
        statement->branches.push_back(parseIfBranch("if"));
        while (tokens.at("elsif"))
            statement->branches.push_back(parseIfBranch("elsif"));
        if (tokens.accept("else"))
            statement->elseBody = parseSequentialStatements({ "end" });
        expectEnd("if", label);
        return statement;
    }

    std::unique_ptr<IfBranch> Parser::parseIfBranch(std::string_view keyword)
    {
        auto branch = node<IfBranch>(tokens.expect(keyword));
        branch->condition = parseExpression();
        tokens.expect("then");
        branch->body = parseSequentialStatements({ "elsif", "else", "end" });
        return branch;
    }

    // ---- case -----------------------------------------------------------------------------------

    /// `case[?] selector is alternative {alternative} end case[?] [label] ;`
    std::unique_ptr<CaseStatement> Parser::parseCase(const std::string& label)
    {
        auto statement = node<CaseStatement>(tokens.expect("case"));
        statement->matching = tokens.accept("?") != nullptr;
        statement->selector = parseExpression();
        tokens.expect("is");
        do statement->alternatives.push_back(parseCaseAlternative());
        while (tokens.at("when"));
        expectEndCase(statement->matching, label);
        return statement;
    }

    /// `when choices => statements`
    std::unique_ptr<CaseAlternative> Parser::parseCaseAlternative()
    {
        auto alternative = node<CaseAlternative>(tokens.expect("when"));
        alternative->choices = parseChoices();
        tokens.expect("=>");
        alternative->body = parseSequentialStatements({ "when", "end" });
        return alternative;
    }

    void Parser::expectEndCase(bool matching, const std::string& label)
    {
        tokens.expect("end");
        tokens.expect("case");
        if (matching)
            tokens.expect("?");
        finishEnd(label);
    }

    // ---- Loops ----------------------------------------------------------------------------------

    /// `for parameter in discrete_range loop ... end loop [label] ;`
    std::unique_ptr<ForLoopStatement> Parser::parseForLoop(const std::string& label)
    {
        auto loop = node<ForLoopStatement>(tokens.expect("for"));
        loop->parameter = tokens.expectIdentifier()->value;
        tokens.expect("in");
        loop->range = parseDiscreteRange();
        loop->body = parseLoopBody(label);
        return loop;
    }

    /// `while condition loop ... end loop [label] ;`
    std::unique_ptr<WhileLoopStatement> Parser::parseWhileLoop(const std::string& label)
    {
        auto loop = node<WhileLoopStatement>(tokens.expect("while"));
        loop->condition = parseExpression();
        loop->body = parseLoopBody(label);
        return loop;
    }

    /// `loop ... end loop [label] ;`
    std::unique_ptr<LoopStatement> Parser::parsePlainLoop(const std::string& label)
    {
        auto loop = node<LoopStatement>(tokens.peek());
        loop->body = parseLoopBody(label);
        return loop;
    }

    std::vector<StatementPtr> Parser::parseLoopBody(const std::string& label)
    {
        tokens.expect("loop");
        std::vector<StatementPtr> body = parseSequentialStatements({ "end" });
        expectEnd("loop", label);
        return body;
    }

    /// `exit [loop_label] [when condition] ;`
    StatementPtr Parser::parseExit()
    {
        auto statement = node<ExitStatement>(tokens.expect("exit"));
        statement->loopLabel = parseOptionalLoopLabel();
        statement->condition = parseOptionalClause("when");
        tokens.expect(";");
        return statement;
    }

    /// `next [loop_label] [when condition] ;`
    StatementPtr Parser::parseNext()
    {
        auto statement = node<NextStatement>(tokens.expect("next"));
        statement->loopLabel = parseOptionalLoopLabel();
        statement->condition = parseOptionalClause("when");
        tokens.expect(";");
        return statement;
    }

    std::string Parser::parseOptionalLoopLabel()
    {
        if (!tokens.atType(TokenType::Identifier))
            return "";
        return tokens.next()->value;
    }

    // ---- Simple statements ----------------------------------------------------------------------

    /// `null ;`
    StatementPtr Parser::parseNull()
    {
        auto statement = node<NullStatement>(tokens.expect("null"));
        tokens.expect(";");
        return statement;
    }

    /// `wait [on names] [until condition] [for timeout] ;`
    StatementPtr Parser::parseWait()
    {
        auto statement = node<WaitStatement>(tokens.expect("wait"));
        statement->onSignals = parseOptionalOnList();
        statement->until = parseOptionalClause("until");
        statement->timeout = parseOptionalClause("for");
        tokens.expect(";");
        return statement;
    }

    std::vector<ExpressionPtr> Parser::parseOptionalOnList()
    {
        std::vector<ExpressionPtr> names;
        if (tokens.accept("on"))
            parseSeparated(",", [&] { names.push_back(parseName()); });
        return names;
    }

    ExpressionPtr Parser::parseOptionalClause(std::string_view keyword)
    {
        if (!tokens.accept(keyword))
            return nullptr;
        return parseExpression();
    }

    /// `report message [severity level] ;`
    StatementPtr Parser::parseReport()
    {
        auto statement = node<ReportStatement>(tokens.expect("report"));
        statement->message = parseExpression();
        statement->severity = parseOptionalClause("severity");
        tokens.expect(";");
        return statement;
    }

    /// `return [value] ;`
    StatementPtr Parser::parseReturn()
    {
        auto statement = node<ReturnStatement>(tokens.expect("return"));
        if (!tokens.at(";"))
            statement->value = parseExpression();
        tokens.expect(";");
        return statement;
    }

} // namespace Pulse::Parser
