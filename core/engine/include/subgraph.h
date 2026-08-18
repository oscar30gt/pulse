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

        /// Builds the subgraph by instantiating its components and wires based on the provided blueprint.
        /// @param bp The blueprint defining the subgraph's architecture.
        /// @param visitedSubgraphs A list of subgraph names that have already been visited to. Used
        /// to detect and prevent infinite recursion when building nested subgraphs.
        /// @throws std::runtime_error if a subgraph is detected to be recursively nested within itself.
        /// @note This function is called internally by the constructor and should not be called after construction.
        void build(const Blueprint& bp, std::vector<std::string>& visitedSubgraphs);


    public:
        Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts);
        // Constructor alternative for internal use when building nested subgraphs.
        Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts, std::vector<std::string>& visitedSubgraphs);
        virtual ~Subgraph() override;

        /// A subgraph time update will propagate the update to all its internal components.
        virtual void update() override;
    };

} // namespace Pulse::Engine

#endif // PULSE_SUBGRAPH_H