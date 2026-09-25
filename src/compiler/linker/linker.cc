#include "linker.h"
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace Pulse::Parser
{
    Linker::Linker() { }

    void Linker::addAST(ASTRoot&& root)
    {
        m_roots.push_back(std::move(root));
    }

    ASTRoot Linker::link()
    {
        ASTRoot linkedDesignRoot;

        std::vector<DesignUnitPtr> entities;
        std::vector<DesignUnitPtr> architectures;
        std::vector<DesignUnitPtr> otherUnits;   // any other design unit: passed through unchanged

        // Sets to track symbol collisions on the fly
        std::unordered_set<std::string> registeredEntities;
        std::unordered_map<std::string, std::unordered_set<std::string>> registeredArchitectures;

        // ----------------------------------------------------------------------------------------
        // Single Pass: Classify, check inline collisions (Point 1 & 2), and transfer ownership
        // ----------------------------------------------------------------------------------------
        for (auto& root : m_roots)
        {
            for (auto& child : root.children)
            {
                if (!child) continue;

                if (auto* entity = dynamic_cast<EntityDeclaration*>(child.get()))
                {
                    // Point 1: Check duplicate entity names
                    if (registeredEntities.contains(entity->name))
                    {
                        throw ast_link_error(
                            "Duplicate entity declaration: '" + entity->name + "'.",
                            entity->source
                        );
                    }

                    registeredEntities.insert(entity->name);
                    entities.push_back(std::move(child));
                }
                
                else if (auto* arch = dynamic_cast<ArchitectureDeclaration*>(child.get()))
                {
                    // Point 2: Check duplicate architecture names for the same target entity
                    if (registeredArchitectures[arch->entityName].contains(arch->name))
                    {
                        throw ast_link_error(
                            "Duplicate architecture '" + arch->name + "' for entity '" + arch->entityName + "'.",
                            arch->source
                        );
                    }

                    registeredArchitectures[arch->entityName].insert(arch->name);
                    architectures.push_back(std::move(child));
                }

                else
                {
                    otherUnits.push_back(std::move(child));
                }
            }
            root.children.clear();
        }

        linkedDesignRoot.children.reserve(otherUnits.size() + entities.size() + architectures.size());

        // Packages and the like go first: they are what entities and architectures refer to.
        std::move(otherUnits.begin(), otherUnits.end(), std::back_inserter(linkedDesignRoot.children));
        std::move(entities.begin(), entities.end(), std::back_inserter(linkedDesignRoot.children));
        std::move(architectures.begin(), architectures.end(), std::back_inserter(linkedDesignRoot.children));

        return linkedDesignRoot;
    }
}