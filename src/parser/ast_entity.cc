#include "ast_internal.h"
#include <algorithm>

namespace Pulse::Parser
{
    std::unique_ptr<EntityDeclaration> ParseContext::parseEntity()
    {
        auto entity = std::make_unique<EntityDeclaration>();

        auto* entityTok = expect("entity");
        entity->source = { entityTok->line, entityTok->column };
        entity->name = expectIdentifier()->value;
        expect("is");

        if (peek()->value == "port")
        {
            entity->ports = parsePortList();
        }

        expect("end");
        maybe("entity");
        maybe(entity->name);
        expect(";");
        return entity;
    }

    std::vector<PortDeclaration> ParseContext::parsePortList()
    {
        std::vector<PortDeclaration> ports;
        expect("port");
        expect("(");

        while (peek()->value != ")")
        {
            PortDeclaration port = parsePortDeclaration();
            ports.push_back(std::move(port));

            if (peek()->value == ";") { 
                next(); 
                continue; 
            }

            break;
        }
    
        expect(")");
        expect(";");
        return ports;
    }

    PortDeclaration ParseContext::parsePortDeclaration()
    {
        PortDeclaration port;
        auto* idTok = expectIdentifier();
        port.source = { idTok->line, idTok->column };
        port.portName = idTok->value;

        expect(":");
        auto* directionTok = expect({ "in", "out", "inout" });
        port.isInput = (directionTok->value == "in" || directionTok->value == "inout");
        port.isOutput = (directionTok->value == "out" || directionTok->value == "inout");

        port.typeSpec = parseTypeSpec();
        return port;
    }
    
} // namespace Pulse::Parser
