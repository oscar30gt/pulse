#include "analyzer_internal.h"

namespace Pulse::Parser
{
    /// A resolution function takes an unconstrained array of the type and returns the type.
    bool AnalyzerContext::isResolutionFunction(const std::string& name, const SemanticType& type) const
    {
        for (const SubprogramInfo* overload : visibleSubprograms(name))
        {
            if (!overload->isFunction || overload->parameters.size() != 1 || overload->returnType.info != type.info)
                continue;

            const FormalInfo& parameter = overload->parameters.front();
            const bool arrayOfType = isOneDimensionalArray(parameter.type) && parameter.type.info->element.info == type.info
                                  && !isConstrainedArray(parameter.type);
            if (arrayOfType && parameter.mode == PortMode::In)
                return true;
        }
        return false;
    }

    /// `f t` resolves the values driven onto a signal of type t; `(f) t` does so for every element of an array type t.
    void AnalyzerContext::applyResolution(SemanticType& type, const Expression& resolution, const ASTNode& at)
    {
        if (auto* element = dynamic_cast<const ElementResolutionExpr*>(&resolution))
        {
            if (!isArray(type))
                fail("An element resolution '(...)' needs an array type, but '" + describe(type) + "' is not one", at);

            SemanticType elementType = type.info->element;
            if (element->resolution)
                applyResolution(elementType, *element->resolution, at);

            type.resolved = true;
            return;
        }

        auto* name = dynamic_cast<const SymbolExpr*>(&resolution);
        if (!name)
            fail("A resolution must name a function", resolution);

        const Symbol* symbol = find(name->name);
        if (!symbol || symbol->kind != SymbolKind::Subprogram)
            fail("'" + name->name + "' is not a function, so it cannot resolve the type '" + describe(type) + "'", resolution);

        if (!isResolutionFunction(name->name, type))
            fail("'" + name->name + "' is not a resolution function for the type '" + type.info->name
                 + "'; a resolution function takes an unconstrained array of that type and returns that type", resolution);

        type.resolved = true;
    }

} // namespace Pulse::Parser
