#include "elaborator_internal.h"

#include <algorithm>
#include <functional>

namespace Pulse::Parser
{
    using namespace Engine;

    namespace
    {
        /// The actual associated with each formal of a map, by formal name: named associations by their formal, positional
        /// ones by the order of `formals`. A formal that is not a plain name (a part, or a conversion) is reported.
        template <typename Formal>
        std::unordered_map<std::string, const Expression*> actualsByName(const std::vector<ExpressionPtr>& map,
                                                                         const std::vector<std::unique_ptr<Formal>>& formals,
                                                                         const std::function<void(const Expression&)>& unsupportedFormal)
        {
            std::unordered_map<std::string, const Expression*> actuals;
            size_t position = 0;
            for (const auto& association : map)
            {
                if (auto* named = dynamic_cast<const NamedAssociationExpr*>(association.get()))
                {
                    auto* formal = dynamic_cast<const SymbolExpr*>(named->formal.get());
                    if (!formal)
                        unsupportedFormal(*named->formal);
                    actuals[formal->name] = named->actual.get();
                    continue;
                }
                if (position < formals.size())
                    actuals[formals[position]->name] = association.get();
                ++position;
            }
            return actuals;
        }
    } // anonymous namespace

    GenericValues UnitElaborator::genericValuesOf(const ComponentInstantiation& instance, const ComponentDeclaration& component,
                                                  const EntityDeclaration& entity)
    {
        const auto actuals = actualsByName(instance.genericMap, component.generics, [&](const Expression& formal)
        {
            unsupported("A generic association to something other than a whole generic", formal);
        });

        // The component's generics take the value the map gives them, or their default (LRM 7.3.3 then gives each entity
        // generic the value of the component generic of the same name).
        std::unordered_map<std::string, StaticValue> values;
        std::vector<const ASTNode*> defaults;
        for (const auto& generic : component.generics)
        {
            auto actual = actuals.find(generic->name);
            const std::string what = "The value of generic '" + generic->name + "' of instance '" + instance.label + "'";

            StaticValue value;
            if (actual != actuals.end() && !dynamic_cast<const OpenExpr*>(actual->second))
                value = requireStatic(*actual->second, what);
            else if (generic->defaultValue)
                value = requireStatic(*generic->defaultValue, what);
            else
                continue;       // the entity's default decides

            // A later default of the component may refer to this generic.
            m_values[generic.get()] = value;
            defaults.push_back(generic.get());
            values[generic->name] = value;
        }
        for (const ASTNode* generic : defaults)
            m_values.erase(generic);

        GenericValues result;
        for (const auto& generic : entity.generics)
            if (auto found = values.find(generic->name); found != values.end())
                result.emplace_back(generic.get(), found->second);
        return result;
    }

    void UnitElaborator::setInitialValue(const ObjectWire& object, LogicVector value)
    {
        if (object.objectClass == ObjectWire::Class::Port)
            m_bp.portWires[object.wire].defaultValue = value;
        else
            m_bp.wires[object.wire].defaultValue = value;
        m_objects[object.declaration].defaultValue = value;
    }

    void UnitElaborator::elaborateInstance(const ComponentInstantiation& instance)
    {
        const ComponentDeclaration* component = m_library.componentOf(instance);
        if (!component)
            fail("Instance '" + instance.label + "' was not analyzed", instance);

        const EntityDeclaration* entity = m_library.entity(component->name);
        if (!entity)
            fail("Component '" + component->name + "' has no entity of the same name to bind instance '" + instance.label + "' to", instance);

        // Default binding (LRM 7.3.3): the most recently analyzed architecture of the entity.
        const ArchitectureDeclaration* architecture = m_library.latestArchitecture(entity->name);
        if (!architecture)
            fail("Entity '" + entity->name + "' has no architecture, so instance '" + instance.label + "' cannot be elaborated", instance);

        const Specialization& child = m_design.specialize(*entity, *architecture, genericValuesOf(instance, *component, *entity), instance);

        const auto actuals = actualsByName(instance.portMap, component->ports, [&](const Expression& formal)
        {
            unsupported("Associating a part of a port, or a port through a conversion function,", formal);
        });

        std::unordered_map<std::string, std::string> portMap;
        for (const auto& port : entity->ports)
        {
            const Layout& layout = child.ports.at(port->name);
            const std::string driver = "instance '" + instance.label + "' (port '" + port->name + "')";
            auto found = actuals.find(port->name);
            const Expression* actual = found != actuals.end() ? found->second : nullptr;

            // An open port (or one the component leaves out) gets a wire of its own, which holds the port's default.
            if (!actual || dynamic_cast<const OpenExpr*>(actual))
            {
                portMap[port->name] = newWire(layout.width, instance.label + "." + port->name, child.portDefaults.at(port->name));
                continue;
            }

            if (port->mode == PortMode::In)
            {
                const ObjectWire* object = objectOf(*actual);
                if (object && object->objectClass != ObjectWire::Class::Variable)
                {
                    if (object->layout.width != layout.width)
                        fail("Port '" + port->name + "' of instance '" + instance.label + "' has " + std::to_string(layout.width)
                             + " bits, but '" + object->name + "' has " + std::to_string(object->layout.width), *actual);
                    portMap[port->name] = object->wire;
                }
                else
                    portMap[port->name] = lowerTo(*actual, layout);
                continue;
            }

            // An output (or inout) drives its actual, which must name a signal or a part of one.
            const TargetPart part = targetPart(*actual);
            if (part.layout.width != layout.width)
                fail("Port '" + port->name + "' of instance '" + instance.label + "' has " + std::to_string(layout.width)
                     + " bits, but its actual has " + std::to_string(part.layout.width), *actual);

            const bool processDriven = child.processDrivenPorts.count(port->name) > 0;
            const LogicVector portDefault = child.portDefaults.at(port->name);
            if (part.whole)
            {
                recordDriver(*part.object, driver, processDriven, *actual);
                portMap[port->name] = part.object->wire;

                // A process of the instance drives the actual's wire directly: until it does, the actual has the value of
                // the port's driver, the port's default (a source would set it the same way).
                if (processDriven)
                    setInitialValue(*part.object, portDefault);
            }
            else
            {
                if (port->mode == PortMode::InOut)
                    unsupported("Connecting an inout port to a part of a signal", *actual);
                const std::string wire = newWire(layout.width, instance.label + "." + port->name, portDefault);
                portMap[port->name] = wire;
                driveTarget(*actual, wire, driver);
            }
        }

        m_bp.addComponent(instance.label, std::make_unique<SubgraphInstance>(child.blueprint, std::move(portMap)));
    }

} // namespace Pulse::Parser
