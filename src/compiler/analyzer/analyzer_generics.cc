#include "analyzer_internal.h"

#include <algorithm>
#include <unordered_set>

namespace Pulse::Parser
{
    // ---- Generics -------------------------------------------------------------------------------

    /// A generic is a constant whose value only an instance knows, so nothing that depends on its value can be checked here.
    std::vector<FormalInfo> AnalyzerContext::resolveGenerics(const std::vector<std::unique_ptr<GenericDeclaration>>& generics)
    {
        std::vector<FormalInfo> formals;

        for (const auto& generic : generics)
        {
            FormalInfo formal;
            formal.name = generic->name;
            formal.kind = SymbolKind::Constant;
            formal.mode = PortMode::In;
            formal.type = resolveTypeSpec(*generic->typeSpec);
            formal.hasDefault = generic->defaultValue != nullptr;
            formal.location = generic->source;

            if (generic->defaultValue)
                checkInitialValue(*generic->defaultValue, formal.type, "generic '" + generic->name + "'");

            formals.push_back(formal);
            declareGenericSymbols({ formal });
        }
        return formals;
    }

    void AnalyzerContext::declareGenericSymbols(const std::vector<FormalInfo>& generics)
    {
        for (const FormalInfo& generic : generics)
        {
            Symbol symbol;
            symbol.kind = SymbolKind::Constant;
            symbol.type = generic.type;
            symbol.mode = PortMode::In;
            symbol.isGeneric = true;
            symbol.objectId = newObjectId();
            declare(generic.name, std::move(symbol), generic.location);
        }
    }

    // ---- Ports ----------------------------------------------------------------------------------

    std::vector<FormalInfo> AnalyzerContext::resolvePorts(const std::vector<std::unique_ptr<PortDeclaration>>& ports,
                                                          const std::string& owner)
    {
        std::vector<FormalInfo> formals;
        std::unordered_set<std::string> names;

        for (const auto& port : ports)
        {
            if (!names.insert(port->name).second)
                fail("Port '" + port->name + "' is declared twice in " + owner, *port);

            FormalInfo formal;
            formal.name = port->name;
            formal.kind = SymbolKind::Signal;
            formal.mode = port->mode;
            formal.type = resolveObjectType(*port->typeSpec, "Port", port->name);
            formal.hasDefault = port->defaultValue != nullptr;
            formal.location = port->source;

            if (port->defaultValue)
                checkInitialValue(*port->defaultValue, formal.type, "port '" + port->name + "'");

            formals.push_back(std::move(formal));
        }
        return formals;
    }

    void AnalyzerContext::declarePortSymbols(const std::vector<FormalInfo>& ports)
    {
        for (const FormalInfo& port : ports)
        {
            Symbol symbol;
            symbol.kind = SymbolKind::Signal;
            symbol.type = port.type;
            symbol.mode = port.mode;
            symbol.isPort = true;
            symbol.objectId = newObjectId();
            declare(port.name, std::move(symbol), port.location);
        }
    }

    // ---- Components against their entity --------------------------------------------------------

    namespace
    {
        /// Same type, and the same length when both lengths are known (a generic-dependent length is not).
        bool sameFormalType(const SemanticType& a, const SemanticType& b)
        {
            if (a.info != b.info)
                return false;

            const auto lengthA = staticLength(a);
            const auto lengthB = staticLength(b);
            return !lengthA || !lengthB || *lengthA == *lengthB;
        }

        const FormalInfo* findFormal(const std::vector<FormalInfo>& formals, const std::string& name)
        {
            auto match = std::find_if(formals.begin(), formals.end(), [&](const FormalInfo& f) { return f.name == name; });
            return match == formals.end() ? nullptr : &*match;
        }
    } // anonymous namespace

    void AnalyzerContext::checkComponentAgainstEntity(const ComponentDeclaration& decl, const EntityInterface& entity,
                                                      const std::vector<FormalInfo>& generics, const std::vector<FormalInfo>& ports)
    {
        for (size_t i = 0; i < generics.size(); ++i)
        {
            const FormalInfo& generic = generics[i];
            const FormalInfo* match = findFormal(entity.generics, generic.name);
            if (!match)
                fail("Generic '" + generic.name + "' of component '" + decl.name + "' does not exist in entity '" + decl.name + "'",
                     *decl.generics[i]);

            if (!sameFormalType(generic.type, match->type))
                fail("Generic '" + generic.name + "' of component '" + decl.name + "' has type '" + describe(generic.type)
                     + "' but the entity declares '" + describe(match->type) + "'", *decl.generics[i]);
        }

        for (size_t i = 0; i < ports.size(); ++i)
        {
            const FormalInfo& port = ports[i];
            const FormalInfo* match = findFormal(entity.ports, port.name);
            if (!match)
                fail("Port '" + port.name + "' of component '" + decl.name + "' does not exist in entity '" + decl.name + "'",
                     *decl.ports[i]);

            if (port.mode != match->mode)
                fail("Port '" + port.name + "' of component '" + decl.name + "' has mode '" + toString(port.mode)
                     + "' but the entity declares '" + toString(match->mode) + "'", *decl.ports[i]);

            if (!sameFormalType(port.type, match->type))
                fail("Port '" + port.name + "' of component '" + decl.name + "' has type '" + describe(port.type)
                     + "' but the entity declares '" + describe(match->type) + "'", *decl.ports[i]);
        }
    }

} // namespace Pulse::Parser
