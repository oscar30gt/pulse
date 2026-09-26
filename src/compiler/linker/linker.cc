#include "linker.h"
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace Pulse::Parser
{
    namespace
    {
        [[noreturn]] void fail(const std::string& message, const ASTNode& node)
        {
            throw ast_link_error(message, node.source);
        }

        /// The type the library resolved for a generic or a port; `what` names it for the message when it was not analyzed.
        const SemanticType& interfaceType(const DesignLibrary& library, const Declaration& decl, const std::string& what)
        {
            const SemanticType* type = library.interfaceType(decl);
            if (!type)
                fail("The " + what + " was not analyzed; analyze its file into the library before linking it", decl);
            return *type;
        }

        /// Same type, and the same length when both lengths are known (a generic-dependent length is not).
        bool sameFormalType(const SemanticType& a, const SemanticType& b)
        {
            if (a.info != b.info)
                return false;

            const auto lengthA = staticLength(a);
            const auto lengthB = staticLength(b);
            return !lengthA || !lengthB || *lengthA == *lengthB;
        }

        /// LRM 6.5.6.3: the entity port is the formal and the component port its actual. An `in` formal takes an `in` or an
        /// `inout` actual, an `out` formal an `out` or an `inout` one, and an `inout` formal only an `inout` one.
        bool modeFits(PortMode entityMode, PortMode componentMode)
        {
            return componentMode == entityMode || componentMode == PortMode::InOut;
        }

        const char* modesThatFit(PortMode entityMode)
        {
            switch (entityMode)
            {
                case PortMode::In:  return "'in' or 'inout'";
                case PortMode::Out: return "'out' or 'inout'";
                default:            return "'inout'";
            }
        }

        // ---- Default binding (LRM 7.3.3) --------------------------------------------------------

        /// The default generic map associates each generic of the component with the generic of the entity of the same
        /// name, which must have the same type; an entity generic the component leaves out takes its default.
        void bindGenerics(const DesignLibrary& library, const ComponentDeclaration& component, const EntityDeclaration& entity)
        {
            std::unordered_map<std::string, const GenericDeclaration*> entityGenerics;
            for (const auto& generic : entity.generics)
                entityGenerics.emplace(generic->name, generic.get());

            std::unordered_set<std::string> declared;
            for (const auto& generic : component.generics)
            {
                declared.insert(generic->name);
                auto match = entityGenerics.find(generic->name);
                if (match == entityGenerics.end())
                    fail("Generic '" + generic->name + "' of component '" + component.name + "' does not exist in entity '" + entity.name + "'",
                         *generic);

                const SemanticType& type = interfaceType(library, *generic, "generic '" + generic->name + "' of component '" + component.name + "'");
                const SemanticType& expected = interfaceType(library, *match->second, "generic '" + generic->name + "' of entity '" + entity.name + "'");
                if (!sameFormalType(type, expected))
                    fail("Generic '" + generic->name + "' of component '" + component.name + "' has type '" + describe(type)
                         + "' but the entity declares '" + describe(expected) + "'", *generic);
            }

            for (const auto& generic : entity.generics)
                if (!declared.count(generic->name) && !generic->defaultValue)
                    fail("Component '" + component.name + "' does not declare the generic '" + generic->name + "' of entity '" + entity.name
                         + "', which has no default value", component);
        }

        /// The default port map associates each port of the component with the port of the entity of the same name, which
        /// must have the same type and a mode the association allows; an entity port the component leaves out stays open,
        /// which an input may only when it has a default (LRM 6.5.6.3).
        void bindPorts(const DesignLibrary& library, const ComponentDeclaration& component, const EntityDeclaration& entity)
        {
            std::unordered_map<std::string, const PortDeclaration*> entityPorts;
            for (const auto& port : entity.ports)
                entityPorts.emplace(port->name, port.get());

            std::unordered_set<std::string> declared;
            for (const auto& port : component.ports)
            {
                declared.insert(port->name);
                auto match = entityPorts.find(port->name);
                if (match == entityPorts.end())
                    fail("Port '" + port->name + "' of component '" + component.name + "' does not exist in entity '" + entity.name + "'", *port);

                const PortDeclaration& formal = *match->second;
                if (!modeFits(formal.mode, port->mode))
                    fail("Port '" + port->name + "' of component '" + component.name + "' has mode '" + toString(port->mode)
                         + "' but the entity declares '" + toString(formal.mode) + "', which only a port of mode " + modesThatFit(formal.mode)
                         + " can be bound to", *port);

                const SemanticType& type = interfaceType(library, *port, "port '" + port->name + "' of component '" + component.name + "'");
                const SemanticType& expected = interfaceType(library, formal, "port '" + port->name + "' of entity '" + entity.name + "'");
                if (!sameFormalType(type, expected))
                    fail("Port '" + port->name + "' of component '" + component.name + "' has type '" + describe(type)
                         + "' but the entity declares '" + describe(expected) + "'", *port);
            }

            for (const auto& port : entity.ports)
                if (!declared.count(port->name) && port->mode == PortMode::In && !port->defaultValue)
                    fail("Component '" + component.name + "' does not declare the input port '" + port->name + "' of entity '" + entity.name
                         + "', which has no default value", component);
        }
    } // anonymous namespace

    // --------------------------------------------------------------------------------------------

    Linker::Linker(const DesignLibrary& library)
        : m_library(library)
    { }

    void Linker::addAST(ASTRoot&& root)
    {
        m_roots.push_back(std::move(root));
    }

    ASTRoot Linker::link()
    {
        // ----------------------------------------------------------------------------------------
        // Binding: every instance to the entity of its component's name, each component checked once.
        // Analysis already made the names of the design units unique.
        // ----------------------------------------------------------------------------------------
        std::unordered_map<std::string, const EntityDeclaration*> entitiesByName;
        for (const auto& root : m_roots)
            for (const auto& child : root.children)
                if (auto* entity = dynamic_cast<const EntityDeclaration*>(child.get()))
                    entitiesByName.emplace(entity->name, entity);

        std::unordered_set<const ComponentDeclaration*> boundComponents;
        for (const auto& root : m_roots)
        {
            for (const auto& child : root.children)
            {
                auto* architecture = dynamic_cast<const ArchitectureDeclaration*>(child.get());
                if (!architecture) continue;

                for (const auto& statement : architecture->body)
                {
                    auto* instance = dynamic_cast<const ComponentInstantiation*>(statement.get());
                    if (!instance) continue;

                    const ComponentDeclaration* component = m_library.componentOf(*instance);
                    if (!component)
                        fail("Instance '" + instance->label + "' was not analyzed; analyze its file into the library before linking it", *instance);

                    auto entity = entitiesByName.find(component->name);
                    if (entity == entitiesByName.end())
                        fail("Component '" + component->name + "' has no entity of the same name, so instance '" + instance->label
                             + "' cannot be bound", *instance);

                    if (boundComponents.insert(component).second)
                    {
                        bindGenerics(m_library, *component, *entity->second);
                        bindPorts(m_library, *component, *entity->second);
                    }
                }
            }
        }

        // ----------------------------------------------------------------------------------------
        // Merging: classify the units and transfer their ownership
        // ----------------------------------------------------------------------------------------
        ASTRoot linkedDesignRoot;

        std::vector<DesignUnitPtr> entities;
        std::vector<DesignUnitPtr> architectures;
        std::vector<DesignUnitPtr> otherUnits;   // any other design unit: passed through unchanged

        for (auto& root : m_roots)
        {
            for (auto& child : root.children)
            {
                if (!child) continue;

                if (dynamic_cast<EntityDeclaration*>(child.get()))
                    entities.push_back(std::move(child));
                else if (dynamic_cast<ArchitectureDeclaration*>(child.get()))
                    architectures.push_back(std::move(child));
                else
                    otherUnits.push_back(std::move(child));
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
