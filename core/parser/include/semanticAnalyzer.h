#ifndef PULSE_VHDL_SEMANTICANALYZER_H
#define PULSE_VHDL_SEMANTICANALYZER_H

#include "AST.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stdexcept>

namespace Pulse::Parser::VHDL
{
    class SemanticAnalyzer
    {
    public:
        SemanticAnalyzer() = default;
        
        // Traverses the AST and throws std::runtime_error on semantic violations
        void analyze(const ASTRoot& root);

    private:
        struct SymbolInfo 
        {
            Pulse::bitWidth_t width;
            bool isInput;
        };

        std::unordered_map<std::string, const EntityDeclaration*> m_globalEntities;
        std::unordered_map<std::string, SymbolInfo> m_currentScopeSymbols;
        std::unordered_map<std::string, const ComponentDeclaration*> m_currentComponents;
        std::unordered_set<std::string> m_processLabels; ///< Process labels seen in the current architecture

        void registerEntities(const ASTRoot& root);
        void analyzeArchitecture(const ArchitectureDeclaration* arch);
        void analyzeAssignment(const SignalAssignment* assignment);
        void analyzeInstantiation(const ComponentInstantiation* inst, const ArchitectureDeclaration* arch);
        void analyzeProcess(const ProcessStatement* proc);
        void analyzeSequentialBody(const std::vector<std::unique_ptr<SequentialStatement>>& body);
        void analyzeSequentialStatement(const SequentialStatement* stmt);
        void analyzeIfStatement(const IfStatement* ifStmt);
        void analyzeCondition(const Expression<ReturnType::BOOLEAN>* cond);

        // Helper to recursively determine the bit-width of an expression
        Pulse::bitWidth_t evaluateWidth(const ASTNode* expr);
        Pulse::bitWidth_t getSignalReferenceWidth(const SignalReference* ref);
    };

} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_SEMANTICANALYZER_H