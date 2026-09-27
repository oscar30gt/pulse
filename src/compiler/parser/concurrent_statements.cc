#include "parser_impl.h"

namespace Pulse::Parser
{
    /// Statements of an architecture body, up to its `end`.
    std::vector<StatementPtr> Parser::parseConcurrentStatements()
    {
        std::vector<StatementPtr> statements;
        while (!tokens.at("end"))
            statements.push_back(parseConcurrentStatement());
        return statements;
    }

    /// `[label :] statement`
    StatementPtr Parser::parseConcurrentStatement()
    {
        const Token* start = tokens.peek();
        std::string label = parseOptionalLabel();
        StatementPtr statement = parseUnlabeledConcurrentStatement(label);
        statement->source = locationOf(start);
        statement->label = std::move(label);
        return statement;
    }

    StatementPtr Parser::parseUnlabeledConcurrentStatement(const std::string& label)
    {
        if (tokens.at("process"))
            return parseProcess(label);
        if (tokens.at("assert"))
            return parseAssert();
        if (tokens.at("with"))
            return parseSelectedAssignment();
        if (tokens.at("component") || (!label.empty() && atInstantiation()))
            return parseInstantiation();
        return parseAssignmentOrCall(false);
    }

    /// After a label: `name generic map`, `name port map` or `name ;` instantiate a component.
    bool Parser::atInstantiation() const
    {
        return tokens.atType(TokenType::Identifier) && (tokens.at("generic", 1) || tokens.at("port", 1) || tokens.at(";", 1));
    }

    // ---- Processes ------------------------------------------------------------------------------

    /// `process [(sensitivity)] [is] declarations begin statements end process [label] ;`
    std::unique_ptr<ProcessStatement> Parser::parseProcess(const std::string& label)
    {
        auto process = node<ProcessStatement>(tokens.expect("process"));
        if (tokens.at("("))
            parseSensitivityList(*process);
        tokens.accept("is");
        process->declarations = parseDeclarativePart(Region::Process);
        tokens.expect("begin");
        process->body = parseSequentialStatements({ "end" });
        expectEnd("process", label);
        return process;
    }

    /// `( all )` or `( name {, name} )`
    void Parser::parseSensitivityList(ProcessStatement& process)
    {
        if (tokens.at("all", 1) && tokens.at(")", 2))
        {
            tokens.expect("(");
            tokens.expect("all");
            tokens.expect(")");
            process.sensitivityAll = true;
            return;
        }
        parseParenthesized([&] { process.sensitivityList.push_back(parseName()); });
    }

    // ---- Component instantiations ---------------------------------------------------------------

    /// `[component] name [generic map (...)] [port map (...)] ;`
    std::unique_ptr<ComponentInstantiation> Parser::parseInstantiation()
    {
        auto instance = node<ComponentInstantiation>(tokens.peek());
        tokens.accept("component");
        instance->componentName = tokens.expectIdentifier()->value;
        instance->genericMap = parseOptionalMap("generic");
        instance->portMap = parseOptionalMap("port");
        tokens.expect(";");
        return instance;
    }

} // namespace Pulse::Parser
