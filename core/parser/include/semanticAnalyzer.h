#ifndef PULSE_VHDL_SEMANTICANALYZER_H
#define PULSE_VHDL_SEMANTICANALYZER_H

#include "AST.h"
#include <unordered_map>
#include <string>
#include <stdexcept>

namespace Pulse::Parser::VHDL
{
    class SemanticAnalyzer
    {
    public:
        SemanticAnalyzer() = default;
        
        // Traverses the AST and throws std::runtime_error on semantic violations
        void analyze(const RootNode& root);

    private:
        struct SymbolInfo 
        {
            Pulse::bitWidth_t width;
            bool isInput;
        };

        std::unordered_map<std::string, const EntityDeclaration*> m_globalEntities;
        std::unordered_map<std::string, SymbolInfo> m_currentScopeSymbols;
        std::unordered_map<std::string, const ComponentDeclaration*> m_currentComponents;

        void registerEntities(const RootNode& root);
        void analyzeArchitecture(const ArchitectureDeclaration* arch);
        void analyzeAssignment(const SignalAssignment* assignment);
        void analyzeInstantiation(const ComponentInstantiation* inst, const ArchitectureDeclaration* arch);

        // Helper to recursively determine the bit-width of an expression
        Pulse::bitWidth_t evaluateWidth(const ASTNode* expr);
        Pulse::bitWidth_t getSignalReferenceWidth(const SignalReference* ref);
    };

} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_SEMANTICANALYZER_H