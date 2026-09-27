#include "printer.h"

namespace Pulse::Parser
{
    using namespace AstPrint;

    // ---- Concurrent statements ------------------------------------------------

    void SignalAssignment::print(int indent) const
    {
        printStatementLine(indent, "SIGNAL ASSIGN", *this);
        printField(indent + 1, "target", target.get());
        printField(indent + 1, "value", value.get());
    }

    void SelectedChoice::print(int indent) const
    {
        printLine(indent, "SELECTED VALUE");
        printField(indent + 1, "value", value.get());
        printField(indent + 1, "when", choices.get());
    }

    void WithClause::print(int indent) const
    {
        printStatementLine(indent, matching ? "WITH SELECT?" : "WITH SELECT", *this);
        printField(indent + 1, "selector", selector.get());
        printField(indent + 1, "target", target.get());
        printList(indent + 1, "choices", choices);
    }

    void ComponentInstantiation::print(int indent) const
    {
        printStatementLine(indent, "INSTANCE", *this);
        printLine(indent + 1, "component", componentName, TYPE);
        printList(indent + 1, "generic map", genericMap);
        printList(indent + 1, "port map", portMap);
    }

    void ProcessStatement::print(int indent) const
    {
        printStatementLine(indent, "PROCESS", *this);
        if (sensitivityAll)
            printLine(indent + 1, "sensitivity", "all", VALUE);
        printList(indent + 1, "sensitivity", sensitivityList);
        printList(indent + 1, "declarations", declarations);
        printList(indent + 1, "body", body);
    }

    // ---- Sequential statements ------------------------------------------------

    void VariableAssignment::print(int indent) const
    {
        printStatementLine(indent, "VARIABLE ASSIGN", *this);
        printField(indent + 1, "target", target.get());
        printField(indent + 1, "value", value.get());
    }

    void IfBranch::print(int indent) const
    {
        printLine(indent, "BRANCH");
        printField(indent + 1, "condition", condition.get());
        printList(indent + 1, "then", body);
    }

    void IfStatement::print(int indent) const
    {
        printStatementLine(indent, "IF", *this);
        printList(indent + 1, "branches", branches);
        printList(indent + 1, "else", elseBody);
    }

    void CaseAlternative::print(int indent) const
    {
        printLine(indent, "WHEN");
        printField(indent + 1, "choices", choices.get());
        printList(indent + 1, "body", body);
    }

    void CaseStatement::print(int indent) const
    {
        printStatementLine(indent, matching ? "CASE?" : "CASE", *this);
        printField(indent + 1, "selector", selector.get());
        printList(indent + 1, "alternatives", alternatives);
    }

    void ForLoopStatement::print(int indent) const
    {
        printStatementLine(indent, "FOR LOOP", *this);
        printLine(indent + 1, "parameter", parameter);
        printField(indent + 1, "range", range.get());
        printList(indent + 1, "body", body);
    }

    void WhileLoopStatement::print(int indent) const
    {
        printStatementLine(indent, "WHILE LOOP", *this);
        printField(indent + 1, "condition", condition.get());
        printList(indent + 1, "body", body);
    }

    void LoopStatement::print(int indent) const
    {
        printStatementLine(indent, "LOOP", *this);
        printList(indent + 1, "body", body);
    }

    void ExitStatement::print(int indent) const
    {
        printStatementLine(indent, "EXIT", *this);
        if (!loopLabel.empty())
            printLine(indent + 1, "loop", loopLabel);
        printOptionalField(indent + 1, "when", condition.get());
    }

    void NextStatement::print(int indent) const
    {
        printStatementLine(indent, "NEXT", *this);
        if (!loopLabel.empty())
            printLine(indent + 1, "loop", loopLabel);
        printOptionalField(indent + 1, "when", condition.get());
    }

    void NullStatement::print(int indent) const
    {
        printStatementLine(indent, "NULL", *this);
    }

    void WaitStatement::print(int indent) const
    {
        printStatementLine(indent, "WAIT", *this);
        printList(indent + 1, "on", onSignals);
        printOptionalField(indent + 1, "until", until.get());
        printOptionalField(indent + 1, "for", timeout.get());
    }

    void AssertStatement::print(int indent) const
    {
        printStatementLine(indent, "ASSERT", *this);
        printField(indent + 1, "condition", condition.get());
        printOptionalField(indent + 1, "report", message.get());
        printOptionalField(indent + 1, "severity", severity.get());
    }

    void ReportStatement::print(int indent) const
    {
        printStatementLine(indent, "REPORT", *this);
        printField(indent + 1, "message", message.get());
        printOptionalField(indent + 1, "severity", severity.get());
    }

    void ReturnStatement::print(int indent) const
    {
        printStatementLine(indent, "RETURN", *this);
        printOptionalField(indent + 1, "value", value.get());
    }

    void ProcedureCallStatement::print(int indent) const
    {
        printStatementLine(indent, "PROCEDURE CALL", *this);
        printChild(call.get(), indent + 1);
    }

} // namespace Pulse::Parser
