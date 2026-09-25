#include "analyzer_internal.h"

namespace Pulse::Parser
{
    // ---- Processes ------------------------------------------------------------------------------

    void AnalyzerContext::declareLabel(const std::string& label, const ASTNode& node)
    {
        if (label.empty())
            return;

        Symbol symbol;
        symbol.kind = SymbolKind::Label;
        declare(label, std::move(symbol), node);
        applyPendingLabelSpecs(label);
    }

    void AnalyzerContext::analyzeProcess(const ProcessStatement& process)
    {
        for (const auto& signal : process.sensitivityList)
        {
            exprType(*signal);
            if (!namesSignal(*signal))
                fail("Only signals and ports can be in a sensitivity list", *signal);
        }

        pushScope();
        m_process = &process;

        analyzeDeclarations(process.declarations);
        analyzeSequence(process.body);
        resolvePendingLabelSpecs();

        m_process = nullptr;
        popScope();
    }

    // ---- Sequential dispatch --------------------------------------------------------------------

    /// Supporting a new sequential statement means adding one line here.
    void AnalyzerContext::registerSequentialHandlers()
    {
        m_sequential.add(*this, &AnalyzerContext::analyzeSignalAssignment);
        m_sequential.add(*this, &AnalyzerContext::analyzeVariableAssignment);
        m_sequential.add(*this, &AnalyzerContext::analyzeWithClause);
        m_sequential.add(*this, &AnalyzerContext::analyzeIf);
        m_sequential.add(*this, &AnalyzerContext::analyzeCase);
        m_sequential.add(*this, &AnalyzerContext::analyzeForLoop);
        m_sequential.add(*this, &AnalyzerContext::analyzeWhileLoop);
        m_sequential.add(*this, &AnalyzerContext::analyzeLoop);
        m_sequential.add(*this, &AnalyzerContext::analyzeExit);
        m_sequential.add(*this, &AnalyzerContext::analyzeNext);
        m_sequential.add(*this, &AnalyzerContext::analyzeNull);
        m_sequential.add(*this, &AnalyzerContext::analyzeWait);
        m_sequential.add(*this, &AnalyzerContext::analyzeAssert);
        m_sequential.add(*this, &AnalyzerContext::analyzeReport);
        m_sequential.add(*this, &AnalyzerContext::analyzeReturn);
        m_sequential.add(*this, &AnalyzerContext::analyzeProcedureCall);
    }

    void AnalyzerContext::analyzeSequence(const std::vector<StatementPtr>& statements)
    {
        for (const auto& statement : statements)
            analyzeSequentialStatement(*statement);
    }

    void AnalyzerContext::analyzeSequentialStatement(const Statement& statement)
    {
        const auto* handler = m_sequential.find(statement);
        if (!handler)
            fail("The analyzer has no handler for this kind of statement in a process or subprogram", statement);

        declareLabel(statement.label, statement);
        (*handler)(statement);
    }

    // ---- Assignments ----------------------------------------------------------------------------

    void AnalyzerContext::analyzeVariableAssignment(const VariableAssignment& assignment)
    {
        if (auto* aggregate = dynamic_cast<const AggregateExpr*>(assignment.target.get()))
        {
            analyzeAggregateTarget(*aggregate, *assignment.value, assignment, false);
            return;
        }

        const SemanticType target = exprType(*assignment.target);
        const RootObject root = rootObject(*assignment.target);

        if (!root)
            fail("The target of ':=' must be a variable", *assignment.target);

        if (root.kind != SymbolKind::Variable)
        {
            const std::string kind = root.kind == SymbolKind::Signal ? "a signal; assign it with '<=' instead of ':='"
                                   : root.kind == SymbolKind::Constant ? "a constant and cannot be assigned"
                                   : root.kind == SymbolKind::LoopParameter ? "a loop parameter and cannot be assigned"
                                   : "not a variable";
            fail("'" + root.name + "' is " + kind, *assignment.target);
        }

        if (root.mode == PortMode::In)
            fail("'" + root.name + "' is an input parameter and cannot be assigned", *assignment.target);

        checkObjectAccess(root, *assignment.target, true);
        analyzeValue(*assignment.value, target, assignment, "variable '" + root.name + "'", false);
    }

    // ---- Selection ------------------------------------------------------------------------------

    void AnalyzerContext::analyzeIf(const IfStatement& statement)
    {
        for (const auto& branch : statement.branches)
        {
            requireCondition(*branch->condition, "the 'if' statement");
            analyzeSequence(branch->body);
        }
        analyzeSequence(statement.elseBody);
    }

    void AnalyzerContext::analyzeCase(const CaseStatement& statement)
    {
        const SemanticType selector = exprType(*statement.selector);

        std::vector<const ChoiceListExpr*> lists;
        for (const auto& alternative : statement.alternatives)
            lists.push_back(&requireChoiceList(*alternative->choices));

        analyzeChoiceLists(lists, selector, *statement.selector, statement.matching ? "the 'case?' statement" : "the 'case' statement",
                           statement.matching);

        for (const auto& alternative : statement.alternatives)
            analyzeSequence(alternative->body);
    }

    // ---- Wait, assert, report, return -----------------------------------------------------------

    void AnalyzerContext::analyzeWait(const WaitStatement& statement)
    {
        if (m_subprogram)
        {
            if (m_subprogram->isFunction)
                fail("A function cannot contain a wait statement", statement);
            m_subprogram->containsWait = true;
        }
        else if (m_process && (!m_process->sensitivityList.empty() || m_process->sensitivityAll))
        {
            fail("A process with a sensitivity list cannot contain wait statements", statement);
        }

        for (const auto& signal : statement.onSignals)
        {
            exprType(*signal);
            if (!namesSignal(*signal))
                fail("Only signals and ports can be waited on", *signal);
        }

        if (statement.until)
            requireCondition(*statement.until, "the 'wait until' clause");

        if (statement.timeout)
            requireTime(*statement.timeout, "timeout of 'wait for'");
    }

    /// The message of `report` and `assert` is a string: a value of the type named `string` that is visible here.
    void AnalyzerContext::analyzeMessage(const Expression& message, const ASTNode& at)
    {
        const Symbol* string = find("string");
        if (!string || string->kind != SymbolKind::Type)
            fail("The message of report and assert is a string, but the type 'string' is not declared", at);

        const SemanticType expected = string->type;
        const SemanticType type = exprType(message, &expected);
        checkAssignable(expected, type, &message, message, "the message");
    }

    void AnalyzerContext::analyzeSeverity(const Expression* severity)
    {
        if (!severity)
            return;

        const SemanticType expected = typeOf(*m_std.severity);
        const SemanticType type = exprType(*severity, &expected);
        if (type.info != m_std.severity)
            fail("The severity must be a 'severity_level' (note, warning, error or failure), but it has type '" + describe(type) + "'", *severity);
    }

    void AnalyzerContext::analyzeReport(const ReportStatement& statement)
    {
        analyzeMessage(*statement.message, statement);
        analyzeSeverity(statement.severity.get());
    }

    void AnalyzerContext::analyzeReturn(const ReturnStatement& statement)
    {
        if (!m_subprogram)
            fail("'return' can only be used inside a subprogram", statement);

        if (!m_subprogram->isFunction)
        {
            if (statement.value)
                fail("The procedure '" + m_subprogram->name + "' cannot return a value", *statement.value);
            return;
        }

        if (!statement.value)
            fail("The function '" + m_subprogram->name + "' must return a value; write 'return <expression>;'", statement);

        const SemanticType& result = m_subprogram->returnType;
        const SemanticType type = exprType(*statement.value, &result);
        checkAssignable(result, type, statement.value.get(), *statement.value, "the result of function '" + m_subprogram->name + "'");
    }

    void AnalyzerContext::analyzeNull(const NullStatement&) { }

} // namespace Pulse::Parser
