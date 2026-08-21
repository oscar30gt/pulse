#ifndef PULSE_VHDL_LINKER_H
#define PULSE_VHDL_LINKER_H

#include "AST.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

namespace Pulse::Parser::VHDL
{
    struct ResolvedInstantiation
    {
        std::string instanceName;
        const EntityDeclaration* targetEntity;
        std::unordered_map<std::string, const ASTNode*> portBindings;
    };

    struct LinkedArchitecture
    {
        std::string architectureName;
        const EntityDeclaration* targetEntity;
        std::vector<const SignalDeclaration*> signals;
        std::vector<const SignalAssignment*> assignments;
        std::vector<ResolvedInstantiation> resolvedInstantiations;
    };

    struct LinkedDesign
    {
        std::unordered_map<std::string, const EntityDeclaration*> entities;
        std::vector<LinkedArchitecture> architectures;
    };

    class Linker
    {
    public:
        Linker() = default;

        // Links multiple source ASTs using non-owning pointers
        LinkedDesign link(const std::vector<const ASTRoot*>& astRoots);

        static void printLinkedDesign(const LinkedDesign& design);

    private:
        std::unordered_map<std::string, const EntityDeclaration*> m_globalEntities;

        void collectEntities(const std::vector<const ASTRoot*>& astRoots);
        void resolveArchitectures(const std::vector<const ASTRoot*>& astRoots, LinkedDesign& outputDesign);
        
        void matchPortSignatures(const ComponentDeclaration* comp, const EntityDeclaration* entity);
    };

} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_LINKER_H