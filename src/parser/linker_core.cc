#include "Linker.h"

namespace Pulse::Parser
{
    LinkedDesign Linker::link(const std::vector<ASTRoot>& astRoots)
    {
        LinkedDesign design;

        // 1. Collect all global entities across all translation units
        collectEntities(astRoots);
        design.entities = m_globalEntities;

        // 2. Resolve architectures, bind instantiations directly to entities, strip component decls
        resolveArchitectures(astRoots, design);

        return design;
    }

    void Linker::collectEntities(const std::vector<ASTRoot>& astRoots)
    {
        m_globalEntities.clear();

        for (const auto& root : astRoots)
        {
            for (const auto& child : root.children)
            {
                if (auto entity = dynamic_cast<const EntityDeclaration*>(child.get()))
                {
                    if (m_globalEntities.find(entity->entityName) != m_globalEntities.end())
                    {
                        throw std::runtime_error("Linker Error: Redefinition of entity '" + entity->entityName + "'");
                    }
                    m_globalEntities[entity->entityName] = entity;
                }
            }
        }
    }

    void Linker::resolveArchitectures(const std::vector<ASTRoot>& astRoots, LinkedDesign& outputDesign)
    {
        for (const auto& root : astRoots)
        {
            for (const auto& child : root.children)
            {
                auto arch = dynamic_cast<const ArchitectureDeclaration*>(child.get());
                if (!arch) continue;

                // Ensure bound entity exists
                auto entityIt = m_globalEntities.find(arch->entityName);
                if (entityIt == m_globalEntities.end())
                {
                    throw std::runtime_error("Linker Error: Architecture '" + arch->architectureName + 
                                             "' targets undefined entity '" + arch->entityName + "'");
                }

                LinkedArchitecture linkedArch;
                linkedArch.architectureName = arch->architectureName;
                linkedArch.targetEntity = entityIt->second;

                // Populate non-owning observer pointers for signals
                for (const auto& sig : arch->signals)
                {
                    linkedArch.signals.push_back(sig.get());
                }

                // Populate non-owning observer pointers for assignments
                for (const auto& assignment : arch->assignments)
                {
                    linkedArch.assignments.push_back(assignment.get());
                }

                // Populate non-owning observer pointers for processes
                for (const auto& proc : arch->processes)
                {
                    linkedArch.processes.push_back(proc.get());
                }

                // Map local components to verify declarations before stripping them
                std::unordered_map<std::string, const ComponentDeclaration*> localComponents;
                for (const auto& comp : arch->components)
                {
                    localComponents[comp->componentName] = comp.get();
                    
                    // Verify component declaration matches a known global entity signature
                    auto matchingEntityIt = m_globalEntities.find(comp->componentName);
                    if (matchingEntityIt == m_globalEntities.end())
                    {
                        throw std::runtime_error("Linker Error: Component declaration '" + comp->componentName + 
                                                 "' has no matching global entity.");
                    }
                    
                    matchPortSignatures(comp.get(), matchingEntityIt->second);
                }

                // Resolve instantiations directly to global entity targets
                for (const auto& inst : arch->instantiations)
                {
                    auto compIt = localComponents.find(inst->componentName);
                    if (compIt == localComponents.end())
                    {
                        throw std::runtime_error("Linker Error: Undeclared component instantiation '" + 
                                                 inst->componentName + "' in instance '" + inst->instanceName + "'");
                    }

                    const EntityDeclaration* targetEntity = m_globalEntities[inst->componentName];

                    ResolvedInstantiation resolvedInst;
                    resolvedInst.instanceName = inst->instanceName;
                    resolvedInst.targetEntity = targetEntity;

                    for (const auto& [portName, exprNode] : inst->portMaps)
                    {
                        resolvedInst.portBindings[portName] = exprNode.get();
                    }

                    linkedArch.resolvedInstantiations.push_back(resolvedInst);
                }

                outputDesign.architectures.push_back(linkedArch);
            }
        }
    }

    void Linker::matchPortSignatures(const ComponentDeclaration* comp, const EntityDeclaration* entity)
    {
        if (comp->ports.size() != entity->ports.size())
        {
            throw std::runtime_error("Linker Error: Port count mismatch between component '" + 
                                     comp->componentName + "' and entity declaration.");
        }

        for (size_t i = 0; i < comp->ports.size(); ++i)
        {
            const auto& compPort = comp->ports[i];
            const auto& entityPort = entity->ports[i];

            if (compPort->portName != entityPort->portName || 
                compPort->width != entityPort->width || 
                compPort->isInput != entityPort->isInput)
            {
                throw std::runtime_error("Linker Error: Port signature mismatch on port '" + 
                                         compPort->portName + "' in component '" + comp->componentName + "'");
            }
        }
    }
} // namespace Pulse::Parser