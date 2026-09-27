#include "analyzer_internal.h"

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
            formal.declaration = generic.get();

            if (generic->defaultValue)
                checkInitialValue(*generic->defaultValue, formal.type, "generic '" + generic->name + "'");

            m_interfaceTypes[generic.get()] = formal.type;
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
            symbol.declaration = generic.declaration;
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
            formal.declaration = port.get();

            if (port->defaultValue)
                checkInitialValue(*port->defaultValue, formal.type, "port '" + port->name + "'");

            m_interfaceTypes[port.get()] = formal.type;
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
            symbol.declaration = port.declaration;
            declare(port.name, std::move(symbol), port.location);
        }
    }

} // namespace Pulse::Parser
