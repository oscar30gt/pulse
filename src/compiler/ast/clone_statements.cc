#include "clone_helpers.h"

namespace Pulse::Parser
{
    // ---- Concurrent statements ------------------------------------------------

    std::unique_ptr<ASTNode> SignalAssignment::clone() const
    {
        auto copy = copyShell(*this);
        copy->target = cloneOf(target);
        copy->value = cloneOf(value);
        return copy;
    }

    std::unique_ptr<ASTNode> SelectedChoice::clone() const
    {
        auto copy = copyShell(*this);
        copy->value = cloneOf(value);
        copy->choices = cloneOf(choices);
        return copy;
    }

    std::unique_ptr<ASTNode> WithClause::clone() const
    {
        auto copy = copyShell(*this);
        copy->matching = matching;
        copy->selector = cloneOf(selector);
        copy->target = cloneOf(target);
        copy->choices = cloneAll(choices);
        return copy;
    }

    std::unique_ptr<ASTNode> ComponentInstantiation::clone() const
    {
        auto copy = copyShell(*this);
        copy->componentName = componentName;
        copy->genericMap = cloneAll(genericMap);
        copy->portMap = cloneAll(portMap);
        return copy;
    }

    std::unique_ptr<ASTNode> ProcessStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->sensitivityAll = sensitivityAll;
        copy->sensitivityList = cloneAll(sensitivityList);
        copy->declarations = cloneAll(declarations);
        copy->body = cloneAll(body);
        return copy;
    }

    // ---- Sequential statements ------------------------------------------------

    std::unique_ptr<ASTNode> VariableAssignment::clone() const
    {
        auto copy = copyShell(*this);
        copy->target = cloneOf(target);
        copy->value = cloneOf(value);
        return copy;
    }

    std::unique_ptr<ASTNode> IfBranch::clone() const
    {
        auto copy = copyShell(*this);
        copy->condition = cloneOf(condition);
        copy->body = cloneAll(body);
        return copy;
    }

    std::unique_ptr<ASTNode> IfStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->branches = cloneAll(branches);
        copy->elseBody = cloneAll(elseBody);
        return copy;
    }

    std::unique_ptr<ASTNode> CaseAlternative::clone() const
    {
        auto copy = copyShell(*this);
        copy->choices = cloneOf(choices);
        copy->body = cloneAll(body);
        return copy;
    }

    std::unique_ptr<ASTNode> CaseStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->matching = matching;
        copy->selector = cloneOf(selector);
        copy->alternatives = cloneAll(alternatives);
        return copy;
    }

    std::unique_ptr<ASTNode> ForLoopStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->parameter = parameter;
        copy->range = cloneOf(range);
        copy->body = cloneAll(body);
        return copy;
    }

    std::unique_ptr<ASTNode> WhileLoopStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->condition = cloneOf(condition);
        copy->body = cloneAll(body);
        return copy;
    }

    std::unique_ptr<ASTNode> LoopStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->body = cloneAll(body);
        return copy;
    }

    std::unique_ptr<ASTNode> ExitStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->loopLabel = loopLabel;
        copy->condition = cloneOf(condition);
        return copy;
    }

    std::unique_ptr<ASTNode> NextStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->loopLabel = loopLabel;
        copy->condition = cloneOf(condition);
        return copy;
    }

    std::unique_ptr<ASTNode> NullStatement::clone() const
    {
        return copyShell(*this);
    }

    std::unique_ptr<ASTNode> WaitStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->onSignals = cloneAll(onSignals);
        copy->until = cloneOf(until);
        copy->timeout = cloneOf(timeout);
        return copy;
    }

    std::unique_ptr<ASTNode> AssertStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->condition = cloneOf(condition);
        copy->message = cloneOf(message);
        copy->severity = cloneOf(severity);
        return copy;
    }

    std::unique_ptr<ASTNode> ReportStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->message = cloneOf(message);
        copy->severity = cloneOf(severity);
        return copy;
    }

    std::unique_ptr<ASTNode> ReturnStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->value = cloneOf(value);
        return copy;
    }

    std::unique_ptr<ASTNode> ProcedureCallStatement::clone() const
    {
        auto copy = copyShell(*this);
        copy->call = cloneOf(call);
        return copy;
    }

} // namespace Pulse::Parser
