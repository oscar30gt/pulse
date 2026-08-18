#ifndef PULSE_SUBGRAPH_H
#define PULSE_SUBGRAPH_H

#include <memory>
#include <unordered_map>

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"
#include "blueprint.h"

namespace Pulse::Engine
{
    /// A dynamically generated subgraph component defined by a blueprint object.
    class Subgraph : public Component
    {
        using Blueprint = Pulse::Parser::Blueprint;

        std::unordered_map<std::string, std::unique_ptr<Wire>> wires;
        std::unordered_map<std::string, std::unique_ptr<Component>> components;

        /// Finds a wire that is accesible within this component.
        /// Wire can come from a port or can be an internal signal.
        Wire* findWire(const std::string& name);

        /// Finds a component instantiated inside this subgraph.
        Component* findComponent(const std::string& name);

    public:
        Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts);
        virtual ~Subgraph() override;
    };

} // namespace Pulse::Engine

#endif // PULSE_SUBGRAPH_H