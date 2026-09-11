#include "parser_internal.h"

namespace Pulse::Parser
{
    std::unique_ptr<SignalAssignment> ParseContext::parseSignalAssignment()
    {
        auto assignment = std::make_unique<SignalAssignment>();
        
        // Parse target expression (signal name, potentially with slicing)
        auto target = parseOperand();
        
        // Expect <=
        auto* leTok = expect("<=");
        assignment->source = { leTok->line, leTok->column };
        assignment->target = std::move(target);
        
        // Parse value expression (with when-else handled transparently)
        assignment->value = parseExpression();
        
        expect(";");
        return assignment;
    }

    std::unique_ptr<ComponentInstantiation> ParseContext::parseComponentInstantiation(const std::string& instanceLabel)
    {
        auto instantiation = std::make_unique<ComponentInstantiation>();
        
        // The instance label was already consumed by the caller
        instantiation->instanceName = instanceLabel;
        
        // Expect component name
        auto* compTok = expectIdentifier();
        instantiation->source = { compTok->line, compTok->column };
        instantiation->componentName = compTok->value;
        
        // Expect port map
        expect("port");
        expect("map");
        expect("(");
        
        // Parse port map entries: portName => signalName {, ...}
        if (peek()->value != ")")
        {
            do
            {
                // Formal port name (left side of =>)
                auto* portTok = expectIdentifier();
                std::string formalPort = portTok->value;
                
                expect("=>");
                
                // Actual signal name (right side of =>)
                auto* sigTok = expectIdentifier();
                auto symbolExpr = SymbolExpr();
                symbolExpr.source = { sigTok->line, sigTok->column };
                symbolExpr.name = sigTok->value;
                
                instantiation->portMap.push_back({ formalPort, symbolExpr });
            } while (maybe(","));
        }
        
        expect(")");
        expect(";");
        
        return instantiation;
    }

} // namespace Pulse::Parser
