#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    // ---- Declarations ---------------------------------------------------------------------------

    /// `attribute name : type_mark;` makes `name` usable in specifications and after a tick.
    void AnalyzerContext::declareAttribute(const AttributeDeclaration& decl)
    {
        Symbol symbol;
        symbol.kind = SymbolKind::Attribute;
        symbol.type = resolveTypeSpec(*decl.typeMark);
        declare(decl.name, std::move(symbol), decl);
    }

    // ---- Specifications -------------------------------------------------------------------------

    bool AnalyzerContext::symbolHasClass(const Symbol& symbol, EntityClass entityClass) const
    {
        const auto hasOverload = [&](bool function)
        {
            return std::any_of(symbol.overloads.begin(), symbol.overloads.end(),
                               [&](const SubprogramInfo* overload) { return overload->isFunction == function; });
        };

        switch (entityClass)
        {
            case EntityClass::Signal:     return symbol.kind == SymbolKind::Signal && !symbol.isPort;
            case EntityClass::Constant:   return symbol.kind == SymbolKind::Constant && !symbol.isGeneric;
            case EntityClass::Variable:   return symbol.kind == SymbolKind::Variable;
            case EntityClass::Type:       return symbol.kind == SymbolKind::Type && symbol.declaresBaseType;
            case EntityClass::Subtype:    return symbol.kind == SymbolKind::Type && !symbol.declaresBaseType;
            case EntityClass::Component:  return symbol.kind == SymbolKind::Component;
            case EntityClass::Label:      return symbol.kind == SymbolKind::Label;
            case EntityClass::Function:   return symbol.kind == SymbolKind::Subprogram && hasOverload(true);
            case EntityClass::Procedure:  return symbol.kind == SymbolKind::Subprogram && hasOverload(false);
            default:                      return false;
        }
    }

    namespace
    {
        const char* describeKind(const Symbol& symbol)
        {
            switch (symbol.kind)
            {
                case SymbolKind::Signal:        return symbol.isPort ? "a port" : "a signal";
                case SymbolKind::Constant:      return symbol.isGeneric ? "a generic" : "a constant";
                case SymbolKind::Variable:      return "a variable";
                case SymbolKind::LoopParameter: return "a loop parameter";
                case SymbolKind::Type:          return symbol.declaresBaseType ? "a type" : "a subtype";
                case SymbolKind::Component:     return "a component";
                case SymbolKind::Label:         return "a label";
                case SymbolKind::Subprogram:    return "a subprogram";
                case SymbolKind::Attribute:     return "an attribute";
            }
            return "something else";
        }
    } // anonymous namespace

    void AnalyzerContext::specifyAttribute(const AttributeSpecification& decl)
    {
        const Symbol* attribute = find(decl.attributeName);
        if (!attribute || attribute->kind != SymbolKind::Attribute)
            fail("'" + decl.attributeName + "' is not an attribute; declare it with 'attribute " + decl.attributeName + " : <type>;' first", decl);

        if (!isStaticExpression(*decl.value))
            fail("The value of an attribute must be constant: it cannot read signals, ports or variables", *decl.value);

        AttributeValue value;
        value.type = attribute->type;
        value.location = decl.source;

        const SemanticType valueType = exprType(*decl.value, &value.type);
        checkAssignable(value.type, valueType, decl.value.get(), *decl.value, "attribute '" + decl.attributeName + "'");
        value.value = fold(*decl.value, &value.type);

        applyAttributeSpecification(decl, value);
    }

    /// Items are given the attribute one by one, or all at once (`all`) / all the remaining ones (`others`) of the class.
    void AnalyzerContext::applyAttributeSpecification(const AttributeSpecification& decl, const AttributeValue& value)
    {
        const bool covering = decl.entities.size() == 1
            && (dynamic_cast<const OthersExpr*>(decl.entities.front().get()) || dynamic_cast<const AllExpr*>(decl.entities.front().get()));

        if (!covering)
        {
            for (const auto& entity : decl.entities)
            {
                auto* name = dynamic_cast<const SymbolExpr*>(entity.get());
                if (!name)
                    fail("'others' and 'all' cannot be mixed with names in an attribute specification", *entity);
                specifyItem(decl, name->name, value, *entity);
            }
            return;
        }

        const std::string& attribute = decl.attributeName;
        Scope& scope = m_scopes.back();

        switch (decl.entityClass)
        {
            case EntityClass::Literal:
            case EntityClass::Units:
                for (const auto& [typeName, symbol] : scope.symbols)
                {
                    if (symbol.kind != SymbolKind::Type || !symbol.declaresBaseType)
                        continue;

                    std::vector<std::string> names = symbol.type.info->literals;
                    if (decl.entityClass == EntityClass::Units)
                    {
                        names.clear();
                        for (const auto& unit : symbol.type.info->units)
                            names.push_back(unit.name);
                    }

                    for (const std::string& item : names)
                        scope.itemAttributes[item].emplace(attribute, value);
                }
                return;

            case EntityClass::Label:
                m_pendingLabelSpecs.push_back({ &decl, "", value, m_scopes.size() - 1 });
                return;

            default:
                break;
        }

        for (auto& [name, symbol] : scope.symbols)
            if (symbolHasClass(symbol, decl.entityClass))
                symbol.attributes.emplace(attribute, value);
    }

    /// Gives one named item its value. The item must be declared in this very region and be of the class the specification names.
    void AnalyzerContext::specifyItem(const AttributeSpecification& decl, const std::string& item, const AttributeValue& value, const ASTNode& at)
    {
        const std::string& attribute = decl.attributeName;

        switch (decl.entityClass)
        {
            // LRM 7.2: the attributes of a design unit are specified in the declarative part of that very unit.
            case EntityClass::Entity:
                fail("An attribute of an entity can only be specified in the declarative part of that entity, so '" + item
                     + "' cannot be given one here", at);

            case EntityClass::Architecture:
                if (!m_architecture || item != m_architecture->name || m_process || m_subprogram)
                    fail("An attribute of an architecture can only be specified in the declarative part of that architecture, so '" + item
                         + "' cannot be given one here", at);
                if (!m_architectureAttributes.insert(attribute).second)
                    fail("The attribute '" + attribute + "' is already specified for '" + item + "'", at);
                return;

            case EntityClass::Configuration: case EntityClass::Package: case EntityClass::Group:
            case EntityClass::File: case EntityClass::Property: case EntityClass::Sequence:
                fail(std::string("'") + item + "' is not " + (decl.entityClass == EntityClass::Configuration ? "a configuration" :
                     decl.entityClass == EntityClass::Package ? "a package" : decl.entityClass == EntityClass::Group ? "a group" :
                     decl.entityClass == EntityClass::File ? "a file" : decl.entityClass == EntityClass::Property ? "a property" : "a sequence")
                     + ": none can be declared in a design", at);

            case EntityClass::Label:
                m_pendingLabelSpecs.push_back({ &decl, item, value, m_scopes.size() - 1 });
                return;

            case EntityClass::Literal: case EntityClass::Units:
            {
                Scope& scope = m_scopes.back();
                bool declaredHere = false;
                for (const auto& [typeName, symbol] : scope.symbols)
                {
                    if (symbol.kind != SymbolKind::Type || !symbol.declaresBaseType)
                        continue;

                    const TypeInfo& info = *symbol.type.info;
                    if (decl.entityClass == EntityClass::Literal)
                        declaredHere = declaredHere || std::find(info.literals.begin(), info.literals.end(), item) != info.literals.end();
                    else
                        declaredHere = declaredHere || std::any_of(info.units.begin(), info.units.end(), [&](const auto& unit) { return unit.name == item; });
                }

                if (!declaredHere)
                    fail("'" + item + "' is not " + (decl.entityClass == EntityClass::Literal ? "an enumeration literal" : "a physical unit")
                         + " of a type declared in this region", at);
                if (!scope.itemAttributes[item].emplace(attribute, value).second)
                    fail("The attribute '" + attribute + "' is already specified for '" + item + "'", at);
                return;
            }

            default:
                break;
        }

        Symbol* symbol = findInCurrentScope(item);
        if (!symbol)
        {
            if (find(item))
                fail("'" + item + "' is declared in an enclosing region; an attribute must be specified in the region that declares it", at);
            fail("'" + item + "' is not declared", at);
        }

        if (!symbolHasClass(*symbol, decl.entityClass))
            fail("'" + item + "' is " + describeKind(*symbol) + ", which is not what the specification names (" + toString(decl.entityClass) + ")", at);

        if (!symbol->attributes.emplace(attribute, value).second)
            fail("The attribute '" + attribute + "' is already specified for '" + item + "'", at);
    }

    /// Labels are declared by the statements, which come after the declarative part the specification is in, so a specification
    /// waits for its label and is applied the moment the label is declared.
    void AnalyzerContext::applyPendingLabelSpecs(const std::string& label)
    {
        const size_t depth = m_scopes.size() - 1;
        Symbol* symbol = findInCurrentScope(label);

        for (PendingLabelSpec& pending : m_pendingLabelSpecs)
        {
            if (pending.depth != depth || (!pending.label.empty() && pending.label != label))
                continue;

            pending.matched = true;
            const std::string& attribute = pending.specification->attributeName;
            if (!symbol->attributes.emplace(attribute, pending.value).second && !pending.label.empty())
                fail("The attribute '" + attribute + "' is already specified for '" + label + "'", pending.specification->source);
        }
    }

    void AnalyzerContext::resolvePendingLabelSpecs()
    {
        const size_t depth = m_scopes.size() - 1;
        std::vector<PendingLabelSpec> remaining;

        for (PendingLabelSpec& pending : m_pendingLabelSpecs)
        {
            if (pending.depth != depth)
            {
                remaining.push_back(std::move(pending));
                continue;
            }

            if (!pending.label.empty() && !pending.matched)
                fail("'" + pending.label + "' is not a label of a statement in this region", pending.specification->source);
        }

        m_pendingLabelSpecs = std::move(remaining);
    }

} // namespace Pulse::Parser
