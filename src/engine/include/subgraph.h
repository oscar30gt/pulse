#ifndef PULSE_SUBGRAPH_H
#define PULSE_SUBGRAPH_H

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

#include "component.h"
#include "eventProbe.h"
#include "signalDrain.h"
#include "signalSource.h"
#include "blueprint.h"

namespace Pulse::Engine
{
    /// A snapshot of a subgraph's state at a specific point in time.
    struct SubgraphSnapshot
    {
        std::unordered_map<std::string, std::pair<bitWidth_t, LogicVector>> inputs;    ///< The state of the subgraph's input ports.
        std::unordered_map<std::string, std::pair<bitWidth_t, LogicVector>> outputs;   ///< The state of the subgraph's output ports.
        std::unordered_map<std::string, std::pair<bitWidth_t, LogicVector>> wires;     ///< The state of the subgraph's internal wires/signals.
        std::unordered_map<std::string, SubgraphSnapshot> subgraphs;   ///< Recursive snapshots of the subgraph's internal subgraph components.
        const SymbolTable* symbols = nullptr;   ///< How to display the signals above (the symbol table of the subgraph's blueprint).
    };

    // --------------------------------------------------------------------------------------------

    /// A dynamically generated subgraph component defined by a blueprint object.
    ///
    /// A design is simulated one tick at a time through the root subgraph's tick(), in steps over the whole hierarchy:
    /// every event probe latches its input (wires know nothing about time), then every probe publishes its 'event, then
    /// every component is updated (processes run), then the processes commit their signal assignments. Each tick is thus
    /// one VHDL-like cycle whose result does not depend on the order in which the components are stored.
    class Subgraph : public Component
    {
        /// Values of the root's port wires (a subgraph inside another one uses its parent's wires).
        std::vector<std::unique_ptr<Wire>> m_ownedPorts;
        std::unordered_map<std::string, std::unique_ptr<Wire>> wires;
        std::unordered_map<std::string, std::unique_ptr<Component>> components;
        /// Event probes, latched and published before the components are updated (they are not in `components`).
        std::vector<std::unique_ptr<EventProbe>> m_probes;
        /// Nested subgraphs by instance name (also in `components`), whose probes latch and publish with ours.
        std::vector<std::pair<std::string, Subgraph*>> m_children;
        /// The blueprint this subgraph was built from; it must outlive the subgraph.
        const Blueprint* m_blueprint;

        /// Port wires a root subgraph creates for itself, built before the Component base.
        struct RootPorts
        {
            std::vector<std::unique_ptr<Wire>> owned;
            PortInitializer inPorts;
            PortInitializer outPorts;
        };
        static RootPorts makeRootPorts(const Blueprint& bp);
        Subgraph(const Blueprint& bp, RootPorts ports);

        /// Finds a wire that is accesible within this component.
        /// Wire can come from a port or can be an internal signal.
        Wire* findWire(const std::string& name);

        /// Same as findWire(), but a missing wire is an error naming what needs it.
        Wire* requireWire(const std::string& name, const std::string& user);

        /// Builds the subgraph by instantiating its components and wires based on the provided blueprint.
        /// @param bp The blueprint defining the subgraph's architecture.
        /// @param visitedBlueprints The blueprints of the subgraphs being built around this one. Used to detect and
        /// prevent infinite recursion when building nested subgraphs.
        /// @throws std::runtime_error if a subgraph is detected to be recursively nested within itself.
        /// @note This function is called internally by the constructor and should not be called after construction.
        void build(const Blueprint& bp, std::vector<const Blueprint*>& visitedBlueprints);

        /// Step 1 of a tick: every event probe of the hierarchy compares its input with the previous tick. Only reads wires.
        void latchEvents();

        /// Step 2 of a tick: every event probe of the hierarchy drives its output.
        void publishEvents();

    public:
        /// A root subgraph: the top of a design. It owns the wires of its ports, built from the blueprint's port info.
        explicit Subgraph(const Blueprint& bp);
        Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts);
        // Constructor alternative for internal use when building nested subgraphs.
        Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts, std::vector<const Blueprint*>& visitedBlueprints);
        virtual ~Subgraph() override;

        /// Simulates one tick of the whole design below this (root) subgraph: events, updates, then commits.
        void tick();

        /// A subgraph time update will propagate the update to all its internal components.
        virtual void update() override;

        /// Commits the postponed effects (process signal assignments) of every internal component.
        virtual void commit() override;

        /// Takes a snapshot of the current state of the subgraph, including the states of all its internal wires and components.
        /// Elements starting with '$' will be ignored. (typically generated during HDL parsing).
        /// @returns SubgraphSnapshot object.
        SubgraphSnapshot takeSnapshot() const;
    };

} // namespace Pulse::Engine

#endif // PULSE_SUBGRAPH_H
