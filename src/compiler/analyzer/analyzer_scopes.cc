#include "analyzer_internal.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace Pulse::Parser
{
    // ---- Diagnostics ----------------------------------------------------------------------------

    void AnalyzerContext::fail(const std::string& message, const SourceLocation& location) const
    {
        throw ast_semantic_error(message, location);
    }

    void AnalyzerContext::fail(const std::string& message, const ASTNode& node) const
    {
        throw ast_semantic_error(message, node.source);
    }

    // ---- Scopes ---------------------------------------------------------------------------------

    void AnalyzerContext::pushScope()
    {
        m_scopes.emplace_back();
    }

    void AnalyzerContext::popScope()
    {
        m_scopes.pop_back();
    }

    TypeInfo& AnalyzerContext::newType(const std::string& name, TypeClass cls)
    {
        m_types.push_back(std::make_unique<TypeInfo>());
        TypeInfo& info = *m_types.back();
        info.name = name;
        info.cls = cls;
        return info;
    }

    // ---- Declaration and lookup ----------------------------------------------------------------

    void AnalyzerContext::declare(const std::string& name, Symbol symbol, const ASTNode& node)
    {
        declare(name, std::move(symbol), node.source);
    }

    void AnalyzerContext::declare(const std::string& name, Symbol symbol, const SourceLocation& location)
    {
        Scope& scope = m_scopes.back();
        symbol.location = location;
        if (symbol.depth == std::numeric_limits<size_t>::max())
            symbol.depth = m_scopes.size() - 1;

        if (scope.symbols.count(name))
            fail("'" + name + "' is already declared in this region", location);

        if (scope.literalsAndUnits.count(name))
            fail("'" + name + "' is already declared in this region as an enumeration literal or a physical unit", location);

        if (symbol.declaresBaseType && symbol.type.info)
        {
            scope.baseTypes.push_back(name);
            scope.literalsAndUnits.insert(symbol.type.info->literals.begin(), symbol.type.info->literals.end());
            for (const auto& unit : symbol.type.info->units)
                scope.literalsAndUnits.insert(unit.name);
        }

        scope.symbols.emplace(name, std::move(symbol));
    }

    Symbol* AnalyzerContext::findInCurrentScope(const std::string& name)
    {
        auto found = m_scopes.back().symbols.find(name);
        return found == m_scopes.back().symbols.end() ? nullptr : &found->second;
    }

    /// The overloads of a name are those of its innermost declaration, plus the ones of outer regions that no inner
    /// declaration hides (a subprogram with the same profile hides the outer one). A declaration of another kind hides all.
    std::vector<const SubprogramInfo*> AnalyzerContext::visibleSubprograms(const std::string& name) const
    {
        std::vector<const SubprogramInfo*> visible;

        for (auto scope = m_scopes.rbegin(); scope != m_scopes.rend(); ++scope)
        {
            auto found = scope->symbols.find(name);
            if (found == scope->symbols.end())
                continue;

            if (found->second.kind != SymbolKind::Subprogram)
                break;

            for (const SubprogramInfo* overload : found->second.overloads)
            {
                const bool hidden = std::any_of(visible.begin(), visible.end(),
                    [&](const SubprogramInfo* inner) { return sameProfile(*inner, *overload); });
                if (!hidden)
                    visible.push_back(overload);
            }
        }
        return visible;
    }

    const Symbol* AnalyzerContext::find(const std::string& name) const
    {
        for (auto scope = m_scopes.rbegin(); scope != m_scopes.rend(); ++scope)
        {
            auto found = scope->symbols.find(name);
            if (found != scope->symbols.end())
                return &found->second;
        }
        return nullptr;
    }

    // ---- Enumeration literals and units ---------------------------------------------------------

    /// Base types visible from the current scope that satisfy `owns`, innermost declarations first.
    /// A name declared in an inner scope hides an outer declaration of the same name.
    template <typename Predicate>
    static std::vector<const TypeInfo*> visibleOwners(const std::vector<Scope>& scopes, Predicate owns)
    {
        std::vector<const TypeInfo*> owners;

        for (size_t depth = scopes.size(); depth-- > 0;)
        {
            for (const std::string& name : scopes[depth].baseTypes)
            {
                const bool hidden = std::any_of(scopes.begin() + static_cast<std::ptrdiff_t>(depth) + 1, scopes.end(),
                                                [&](const Scope& inner) { return inner.symbols.count(name) > 0; });
                const Symbol& symbol = scopes[depth].symbols.at(name);
                if (!hidden && owns(*symbol.type.info))
                    owners.push_back(symbol.type.info);
            }
        }
        return owners;
    }

    std::vector<const TypeInfo*> AnalyzerContext::enumerationOwners(const std::string& literal) const
    {
        return visibleOwners(m_scopes, [&](const TypeInfo& info)
        {
            return info.cls == TypeClass::Enumeration
                && std::find(info.literals.begin(), info.literals.end(), literal) != info.literals.end();
        });
    }

    const TypeInfo* AnalyzerContext::pickEnumerationOwner(const std::string& literal, const SemanticType* expected) const
    {
        const auto owners = enumerationOwners(literal);

        if (expected && expected->info && std::find(owners.begin(), owners.end(), expected->info) != owners.end())
            return expected->info;

        return owners.size() == 1 ? owners.front() : nullptr;
    }

    std::optional<UnitRef> AnalyzerContext::findUnit(const std::string& unit) const
    {
        auto owners = visibleOwners(m_scopes, [&](const TypeInfo& info)
        {
            return std::any_of(info.units.begin(), info.units.end(), [&](const auto& u) { return u.name == unit; });
        });

        if (owners.empty())
            return std::nullopt;

        // The owner was chosen because it has a unit of this name.
        const TypeInfo* owner = owners.front();
        const auto found = std::find_if(owner->units.begin(), owner->units.end(), [&](const auto& u) { return u.name == unit; });
        return UnitRef{ owner, found->factor };
    }

} // namespace Pulse::Parser
