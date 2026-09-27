#include "analyzer_internal.h"

#include <algorithm>
#include <functional>

namespace Pulse::Parser
{
    /// Adding support for a new expression node means adding one line here.
    void AnalyzerContext::registerExpressionHandlers()
    {
        m_expressions.add(*this, &AnalyzerContext::typeOfSymbol);
        m_expressions.add(*this, &AnalyzerContext::typeOfField);
        m_expressions.add(*this, &AnalyzerContext::typeOfCall);
        m_expressions.add(*this, &AnalyzerContext::typeOfAttribute);
        m_expressions.add(*this, &AnalyzerContext::typeOfQualified);
        m_expressions.add(*this, &AnalyzerContext::typeOfExternalName);
        m_expressions.add(*this, &AnalyzerContext::typeOfInteger);
        m_expressions.add(*this, &AnalyzerContext::typeOfReal);
        m_expressions.add(*this, &AnalyzerContext::typeOfPhysical);
        m_expressions.add(*this, &AnalyzerContext::typeOfCharacter);
        m_expressions.add(*this, &AnalyzerContext::typeOfString);
        m_expressions.add(*this, &AnalyzerContext::typeOfAggregate);
        m_expressions.add(*this, &AnalyzerContext::typeOfUnary);
        m_expressions.add(*this, &AnalyzerContext::typeOfBinary);

        // Nodes that only make sense inside another construct: using them as a value is an error of its own.
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<SignatureExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<NamedAssociationExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<OpenExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<TypeSpec>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<ElementResolutionExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<OthersExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<AllExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<ChoiceListExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<WhenElseExpr>);
        m_expressions.add(*this, &AnalyzerContext::typeOfNonValue<UnaffectedExpr>);
    }

    SemanticType AnalyzerContext::exprType(const Expression& expr, const SemanticType* expected)
    {
        if (m_expressionDepth >= maxExpressionDepth)
            fail("This expression is nested too deeply to analyze (more than " + std::to_string(maxExpressionDepth)
                 + " levels); split it into several statements", expr);

        DepthGuard guard(m_expressionDepth);

        const auto* handler = m_expressions.find(expr);
        if (!handler)
            fail("The analyzer has no handler for this kind of expression", expr);

        SemanticType type = (*handler)(expr, expected);
        m_expressionTypes[&expr] = type;
        return type;
    }

    void AnalyzerContext::rejectNonValue(const Expression& expr) const
    {
        if (dynamic_cast<const SignatureExpr*>(&expr))
            fail("A signature is not a value; it can only follow the name of a subprogram or an enumeration literal", expr);
        if (dynamic_cast<const NamedAssociationExpr*>(&expr))
            fail("An association with '=>' is not a value; it can only appear in a call, a map or an aggregate", expr);
        if (dynamic_cast<const OpenExpr*>(&expr))
            fail("'open' is not a value; it can only be an actual in a map or a call, or an unconstrained index constraint", expr);
        if (dynamic_cast<const TypeSpec*>(&expr))
            fail("A type or subtype indication is not a value", expr);
        if (dynamic_cast<const ElementResolutionExpr*>(&expr))
            fail("A resolution indication is not a value; it can only precede a type mark", expr);
        if (dynamic_cast<const OthersExpr*>(&expr))
            fail("'others' is not a value; it can only be a choice", expr);
        if (dynamic_cast<const AllExpr*>(&expr))
            fail("'all' is not a value; it can only name every item of an attribute specification", expr);
        if (dynamic_cast<const ChoiceListExpr*>(&expr))
            fail("A list of choices is not a value; it can only follow 'when' or precede '=>' in an aggregate", expr);
        if (dynamic_cast<const WhenElseExpr*>(&expr))
            fail("A 'when ... else' value can only be the right-hand side of an assignment", expr);
        if (dynamic_cast<const UnaffectedExpr*>(&expr))
            fail("'unaffected' can only be the value of a signal assignment", expr);

        fail("This construct is not a value", expr);
    }

    // ---- Names ----------------------------------------------------------------------------------

    SemanticType AnalyzerContext::typeOfSymbol(const SymbolExpr& expr, const SemanticType* expected)
    {
        if (const Symbol* symbol = find(expr.name))
        {
            switch (symbol->kind)
            {
                case SymbolKind::Type:
                    fail("'" + expr.name + "' is a type, not a value", expr);
                case SymbolKind::Component:
                    fail("'" + expr.name + "' is a component, not a value", expr);
                case SymbolKind::Label:
                    fail("'" + expr.name + "' is a label, not a value", expr);
                case SymbolKind::Attribute:
                    fail("'" + expr.name + "' is an attribute, not a value; write it after a tick, e.g. x'" + expr.name, expr);
                case SymbolKind::Subprogram:
                    return typeOfFunctionCall(expr.name, expr, {}, expected);   // a call without arguments
                default:
                    break;
            }

            const RootObject root = rootObject(expr);
            checkObjectAccess(root, expr, false);
            return symbol->type;
        }

        return enumerationLiteralType(expr.name, expected, expr);
    }

    SemanticType AnalyzerContext::typeOfField(const FieldAccessExpr& expr, const SemanticType*)
    {
        if (expr.fieldName == "all")
            fail("'.all' needs an access type, and access types are not supported", expr);

        const SemanticType record = exprType(*expr.target);

        if (!isRecord(record))
            fail("'" + expr.fieldName + "' cannot be selected: the prefix has type '" + describe(record) + "', which is not a record", expr);

        for (const auto& field : record.info->fields)
            if (field.name == expr.fieldName)
                return field.type;

        std::string available;
        for (const auto& field : record.info->fields)
            available += (available.empty() ? "" : ", ") + field.name;

        fail("Record type '" + record.info->name + "' has no field '" + expr.fieldName + "' (fields: " + available + ")", expr);
    }

    // ---- Root objects ---------------------------------------------------------------------------

    RootObject AnalyzerContext::rootObject(const Expression& name) const
    {
        const Expression* current = &name;
        bool whole = true;

        while (current)
        {
            if (auto* identifier = dynamic_cast<const SymbolExpr*>(current))
            {
                const Symbol* symbol = find(identifier->name);
                if (!symbol)
                    return {};

                if (symbol->declaration)
                    m_denotations[identifier] = symbol->declaration;

                RootObject root;
                root.node = identifier;
                root.symbol = symbol;
                root.name = identifier->name;
                root.kind = symbol->kind;
                root.mode = symbol->mode;
                root.objectId = symbol->objectId;
                root.depth = symbol->depth;
                root.whole = whole && !symbol->partialAlias;
                root.isPort = symbol->isPort;
                root.isParameter = symbol->isParameter;
                return root;
            }

            if (auto* external = dynamic_cast<const ExternalNameExpr*>(current))
            {
                RootObject root;
                root.node = external;
                root.name = external->path;
                root.kind = external->objectClass == ExternalObjectClass::Signal ? SymbolKind::Signal
                          : external->objectClass == ExternalObjectClass::Variable ? SymbolKind::Variable
                          : SymbolKind::Constant;
                root.objectId = std::hash<std::string>{}(external->path) | (size_t(1) << (sizeof(size_t) * 8 - 1));
                root.depth = 0;
                root.whole = whole;
                return root;
            }

            whole = false;
            if (auto* call = dynamic_cast<const FunctionCallExpr*>(current)) current = call->callee.get();
            else if (auto* field = dynamic_cast<const FieldAccessExpr*>(current)) current = field->target.get();
            else return {};
        }
        return {};
    }

    void AnalyzerContext::checkObjectAccess(const RootObject& root, const ASTNode& at, bool)
    {
        if (!m_subprogram || !root || !m_subprogram->isFunction || m_subprogram->impure)
            return;

        const bool stateful = root.kind == SymbolKind::Signal || root.kind == SymbolKind::Variable;
        if (stateful && root.depth < m_bodyDepth)
            fail("The pure function '" + m_subprogram->name + "' cannot access '" + root.name
                 + "', which is declared outside it; declare the function 'impure' to allow it", at);
    }

    // ---- Context and staticness -----------------------------------------------------------------

    /// True for expressions whose type cannot be known without the type the context expects.
    bool AnalyzerContext::needsContext(const Expression& expr) const
    {
        if (dynamic_cast<const CharacterLiteralExpr*>(&expr) || dynamic_cast<const StringLiteralExpr*>(&expr)
            || dynamic_cast<const AggregateExpr*>(&expr))
            return true;

        if (auto* symbol = dynamic_cast<const SymbolExpr*>(&expr))
            return !find(symbol->name) && !enumerationOwners(symbol->name).empty();

        return false;
    }

    namespace
    {
        template <typename List, typename Predicate>
        bool allOf(const List& list, Predicate predicate)
        {
            return std::all_of(list.begin(), list.end(), predicate);
        }
    } // anonymous namespace

    /// True when the expression reads no signal, port, variable or loop parameter (it is constant for an instance).
    bool AnalyzerContext::isStaticExpression(const Expression& expr) const
    {
        if (m_expressionDepth >= maxExpressionDepth)
            fail("This expression is nested too deeply to analyze (more than " + std::to_string(maxExpressionDepth)
                 + " levels); split it into several statements", expr);

        DepthGuard guard(m_expressionDepth);
        const auto staticOf = [&](const ExpressionPtr& e) { return !e || isStaticExpression(*e); };

        // Every pure function of static arguments is static; so is a call of a name that only has pure functions.
        const auto pureOverloads = [&](const std::string& name)
        {
            const auto overloads = visibleSubprograms(name);
            return !overloads.empty() && std::all_of(overloads.begin(), overloads.end(),
                [](const SubprogramInfo* info) { return info->isFunction && !info->impure; });
        };

        if (auto* n = dynamic_cast<const SymbolExpr*>(&expr))
        {
            const Symbol* symbol = find(n->name);
            if (symbol && symbol->kind == SymbolKind::Subprogram)
                return pureOverloads(n->name);
            return !symbol || (symbol->kind != SymbolKind::Signal && symbol->kind != SymbolKind::Variable
                               && symbol->kind != SymbolKind::LoopParameter);
        }

        if (auto* n = dynamic_cast<const UnaryOpExpr*>(&expr)) return staticOf(n->operand);
        if (auto* n = dynamic_cast<const FieldAccessExpr*>(&expr)) return staticOf(n->target);
        if (auto* n = dynamic_cast<const PhysicalLiteralExpr*>(&expr)) return staticOf(n->magnitude);
        if (auto* n = dynamic_cast<const AttributeExpr*>(&expr)) return n->attributeName != "event";
        if (auto* n = dynamic_cast<const QualifiedExpr*>(&expr)) return staticOf(n->operand);
        if (auto* n = dynamic_cast<const NamedAssociationExpr*>(&expr)) return staticOf(n->actual);
        if (auto* n = dynamic_cast<const ExternalNameExpr*>(&expr)) return n->objectClass == ExternalObjectClass::Constant;

        if (auto* n = dynamic_cast<const BinaryOpExpr*>(&expr))
            return staticOf(n->left) && staticOf(n->right);

        if (auto* n = dynamic_cast<const WhenElseExpr*>(&expr))
            return staticOf(n->trueValue) && staticOf(n->condition) && staticOf(n->falseValue);

        if (auto* n = dynamic_cast<const FunctionCallExpr*>(&expr))
        {
            auto* callee = dynamic_cast<const SymbolExpr*>(n->callee.get());
            const Symbol* symbol = callee ? find(callee->name) : nullptr;
            const bool calleeStatic = symbol && symbol->kind == SymbolKind::Subprogram ? pureOverloads(callee->name) : staticOf(n->callee);
            return calleeStatic && allOf(n->arguments, staticOf);
        }

        if (auto* n = dynamic_cast<const AggregateExpr*>(&expr))
            return allOf(n->elements, staticOf);

        if (auto* n = dynamic_cast<const ChoiceListExpr*>(&expr))
            return allOf(n->alternatives, staticOf);

        return true;
    }

} // namespace Pulse::Parser
