#include "analyzer_internal.h"

namespace Pulse::Parser
{
    void AnalyzerContext::analyzeConcurrentStatement(const Statement& statement)
    {
        const auto* handler = m_concurrent.find(statement);
        if (!handler)
            fail("The analyzer has no handler for this kind of statement in an architecture", statement);

        declareLabel(statement.label, statement);
        beginDriverSource(statement);
        (*handler)(statement);
    }

    // ---- Assignment targets ---------------------------------------------------------------------

    /// A signal assignment may only drive signals and output ports (or the signal parameters of the subprogram being analyzed).
    void AnalyzerContext::checkWritable(const Expression& target, const std::string& what)
    {
        const RootObject root = rootObject(target);
        if (!root)
            fail("The target of " + what + " must be a signal or a port", target);

        const std::string& name = root.name;
        switch (root.kind)
        {
            case SymbolKind::Signal:
                if (root.mode == PortMode::In)
                    fail(std::string(root.isParameter ? "Parameter '" : "Port '") + name + "' is an input and cannot be assigned", target);

                checkObjectAccess(root, target, true);
                if (m_subprogram && !root.isParameter && !m_bodyInProcess)
                    fail("The subprogram '" + m_subprogram->name + "' is not declared in a process, so it can only assign its own signal "
                         "parameters, not '" + name + "'", target);
                return;

            case SymbolKind::Variable:
                fail("'" + name + "' is a variable; assign it with ':=' instead of '<='", target);

            case SymbolKind::Constant:
                fail("'" + name + "' is a constant and cannot be assigned", target);

            case SymbolKind::LoopParameter:
                fail("'" + name + "' is a loop parameter and cannot be assigned", target);

            default:
                fail("The target of " + what + " must be a signal or a port, but '" + name + "' is not an object", target);
        }
    }

    void AnalyzerContext::analyzeAggregateTarget(const AggregateExpr& target, const Expression& value, const ASTNode& at, bool signalTarget)
    {
        const TypeInfo* elementInfo = nullptr;

        for (const auto& element : target.elements)
        {
            if (dynamic_cast<const NamedAssociationExpr*>(element.get()))
                fail("Named elements are not supported in an aggregate target; list the targets in order", *element);

            const SemanticType elementType = exprType(*element);
            if (signalTarget)
            {
                checkWritable(*element, "an aggregate signal assignment");
                recordDriver(*element, at);
            }
            else
            {
                const RootObject root = rootObject(*element);
                if (!root || root.kind != SymbolKind::Variable)
                    fail("Every element of an aggregate target of ':=' must be a variable", *element);
            }

            if (elementInfo && elementInfo != elementType.info)
                fail("The elements of an aggregate target must all have the same type, but found '" + describe(elementType)
                     + "' and '" + elementInfo->name + "'", *element);
            elementInfo = elementType.info;
        }

        const auto checkValue = [&](const Expression& single)
        {
            if (dynamic_cast<const UnaffectedExpr*>(&single))
            {
                if (!signalTarget)
                    fail("'unaffected' can only be the value of a signal assignment", single);
                return;
            }

            const SemanticType valueType = exprType(single);
            if (!isOneDimensionalArray(valueType) || valueType.info->element.info != elementInfo)
                fail("The value of an aggregate target of " + std::to_string(target.elements.size()) + " '" + (elementInfo ? elementInfo->name : "?")
                     + "' element(s) must be an array of '" + (elementInfo ? elementInfo->name : "?") + "', but it has type '" + describe(valueType) + "'", single);

            const auto length = staticLength(valueType);
            if (length && *length != static_cast<int64_t>(target.elements.size()))
                fail("The aggregate target has " + std::to_string(target.elements.size()) + " element(s), but the value '" + describe(valueType)
                     + "' has " + std::to_string(*length), single);
        };

        // The value is a plain expression or a chain `v1 when c1 else v2 ...`, walked without recursion like analyzeValue does.
        const Expression* current = &value;
        while (auto* conditional = dynamic_cast<const WhenElseExpr*>(current))
        {
            checkValue(*conditional->trueValue);
            requireCondition(*conditional->condition, "the 'when' clause");

            if (!conditional->falseValue)
                return;
            current = conditional->falseValue.get();
        }
        checkValue(*current);
    }

    // ---- Values ---------------------------------------------------------------------------------

    /// A value is a plain expression, or a chain `v1 when c1 else v2 when c2 else v3`, walked without recursion so a
    /// long chain cannot exhaust the stack.
    void AnalyzerContext::analyzeValue(const Expression& value, const SemanticType& target, const ASTNode& at, const std::string& what,
                                       bool allowUnaffected)
    {
        const auto analyzeOne = [&](const Expression& single)
        {
            if (dynamic_cast<const UnaffectedExpr*>(&single))
            {
                if (!allowUnaffected)
                    fail("'unaffected' can only be the value of a signal assignment", single);
                return;
            }

            const SemanticType valueType = exprType(single, &target);
            checkAssignable(target, valueType, &single, single, what);
        };

        (void)at;
        const Expression* current = &value;
        while (auto* conditional = dynamic_cast<const WhenElseExpr*>(current))
        {
            analyzeOne(*conditional->trueValue);
            requireCondition(*conditional->condition, "the 'when' clause");

            if (!conditional->falseValue)
                return;
            current = conditional->falseValue.get();
        }
        analyzeOne(*current);
    }

    // ---- Signal assignment ----------------------------------------------------------------------

    void AnalyzerContext::analyzeSignalAssignment(const SignalAssignment& assignment)
    {
        if (m_subprogram)
        {
            if (m_subprogram->isFunction)
                fail("A function cannot assign signals", assignment);
            m_subprogram->assignsSignals = true;
        }

        if (auto* aggregate = dynamic_cast<const AggregateExpr*>(assignment.target.get()))
        {
            analyzeAggregateTarget(*aggregate, *assignment.value, assignment, true);
            return;
        }

        const SemanticType target = exprType(*assignment.target);
        checkWritable(*assignment.target, "a signal assignment");
        recordDriver(*assignment.target, assignment);

        const RootObject root = rootObject(*assignment.target);
        analyzeValue(*assignment.value, target, assignment, "signal '" + root.name + "'", true);
    }

    // ---- Assertions and procedure calls ---------------------------------------------------------

    void AnalyzerContext::analyzeAssert(const AssertStatement& statement)
    {
        requireCondition(*statement.condition, "the 'assert' statement");
        if (statement.message)
            analyzeMessage(*statement.message, statement);
        analyzeSeverity(statement.severity.get());
    }

    void AnalyzerContext::analyzeProcedureCall(const ProcedureCallStatement& statement)
    {
        const Expression& call = *statement.call;
        std::vector<const Expression*> arguments;
        const SymbolExpr* name = dynamic_cast<const SymbolExpr*>(&call);

        if (auto* callWithArguments = dynamic_cast<const FunctionCallExpr*>(&call))
        {
            name = dynamic_cast<const SymbolExpr*>(callWithArguments->callee.get());
            arguments = associationsOf(callWithArguments->arguments);
        }

        if (!name)
            fail("A procedure call must name the procedure it calls", call);

        const Symbol* symbol = find(name->name);
        if (!symbol)
            fail("'" + name->name + "' is not declared", call);
        if (symbol->kind != SymbolKind::Subprogram)
            fail("'" + name->name + "' is not a procedure", call);

        resolveCall(name->name, call, arguments, nullptr, false);
    }

} // namespace Pulse::Parser
