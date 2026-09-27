#include "analyzer_internal.h"

namespace Pulse::Parser
{
    namespace
    {
        bool isRangeAttribute(const std::string& name) { return name == "range" || name == "reverse_range"; }
        bool isBoundAttribute(const std::string& name) { return name == "left" || name == "right" || name == "high" || name == "low"; }

        /// The attributes the analyzer knows without a declaration; every other name must be declared by the design.
        bool isPredefinedAttribute(const std::string& name)
        {
            return name == "event" || name == "length" || isBoundAttribute(name) || isRangeAttribute(name);
        }
    } // anonymous namespace

    bool AnalyzerContext::isTypeName(const Expression& expr) const
    {
        auto* symbol = dynamic_cast<const SymbolExpr*>(&expr);
        if (!symbol) return false;

        const Symbol* found = find(symbol->name);
        return found && found->kind == SymbolKind::Type;
    }

    /// The type an attribute applies to: the named type itself, or the type of the named object.
    SemanticType AnalyzerContext::attributePrefixType(const Expression& prefix)
    {
        if (isTypeName(prefix))
            return resolveTypeName(static_cast<const SymbolExpr&>(prefix).name, prefix);

        return exprType(prefix);
    }

    bool AnalyzerContext::namesSignal(const Expression& expr) const
    {
        const RootObject root = rootObject(expr);
        return root && root.kind == SymbolKind::Signal;
    }

    // ---- Predefined value attributes: 'event 'length 'left 'right 'high 'low -----------------------

    SemanticType AnalyzerContext::typeOfAttribute(const AttributeExpr& expr, const SemanticType*)
    {
        const std::string& name = expr.attributeName;

        if (!isPredefinedAttribute(name))
            return typeOfUserAttribute(expr);

        if (expr.signature)
            fail("The predefined attribute '" + name + " does not take a signature", expr);

        if (isRangeAttribute(name))
            fail("'" + name + " can only be used where a range is expected, e.g. 'for i in v'range loop' or an index constraint", expr);

        const SemanticType prefix = attributePrefixType(*expr.prefix);

        if (name == "event")
        {
            if (!namesSignal(*expr.prefix))
                fail("'event can only be applied to a signal or a port", expr);
            return typeOf(*m_std.boolean);
        }

        if (name == "length")
        {
            if (!isArray(prefix))
                fail("'length needs an array, but the prefix has type '" + describe(prefix) + "'", expr);
            return universalInteger();
        }

        const bool boundsOfArray = isArray(prefix);
        if (!boundsOfArray && !(isScalar(prefix) && isTypeName(*expr.prefix)))
            fail("'" + name + " needs an array or a scalar type, but the prefix has type '" + describe(prefix) + "'", expr);

        return boundsOfArray ? prefix.info->indexTypes.front() : withoutConstraint(prefix);
    }

    // ---- Attributes the design declares ---------------------------------------------------------

    /// The value a specification gave the item a plain name denotes, if any.
    const AttributeValue* AnalyzerContext::specifiedAttribute(const Expression& prefix, const std::string& attribute) const
    {
        auto* name = dynamic_cast<const SymbolExpr*>(&prefix);
        if (!name)
            return nullptr;

        if (const Symbol* symbol = find(name->name))
        {
            auto found = symbol->attributes.find(attribute);
            return found == symbol->attributes.end() ? nullptr : &found->second;
        }

        // Enumeration literals and physical units are not symbols; their attributes live in the region of their type.
        for (auto scope = m_scopes.rbegin(); scope != m_scopes.rend(); ++scope)
        {
            auto item = scope->itemAttributes.find(name->name);
            if (item == scope->itemAttributes.end())
                continue;

            auto found = item->second.find(attribute);
            if (found != item->second.end())
                return &found->second;
        }
        return nullptr;
    }

    SemanticType AnalyzerContext::typeOfUserAttribute(const AttributeExpr& expr)
    {
        const std::string& attributeName = expr.attributeName;
        const Symbol* declared = find(attributeName);
        if (!declared || declared->kind != SymbolKind::Attribute)
            fail("Unknown attribute '" + attributeName + "'", expr);

        auto* name = dynamic_cast<const SymbolExpr*>(expr.prefix.get());
        if (!name)
            fail("The attribute '" + attributeName + "' can only be read from a plain name, not from an expression", expr);

        const Symbol* item = find(name->name);
        if (!item && enumerationOwners(name->name).empty() && !findUnit(name->name))
            fail("'" + name->name + "' is not declared", *expr.prefix);

        if (expr.signature)
        {
            if (!item || item->kind != SymbolKind::Subprogram)
                fail("A signature can only follow the name of a subprogram", *expr.signature);
            selectBySignature(visibleSubprograms(name->name), *expr.signature, *expr.signature);
        }

        const AttributeValue* value = specifiedAttribute(*expr.prefix, attributeName);
        if (!value)
            fail("The attribute '" + attributeName + "' has no value for '" + name->name + "'; specify it with 'attribute "
                 + attributeName + " of " + name->name + " : <class> is <value>;'", expr);

        return value->type;
    }

} // namespace Pulse::Parser
