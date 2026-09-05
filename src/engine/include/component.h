#ifndef PULSE_COMPONENT_H
#define PULSE_COMPONENT_H

#include <string>
#include <vector>

#include "wire.h"

namespace Pulse::Engine
{
    /// Base class for all components in the Pulse engine. 
    /// A component can have multiple input and output ports, each connected to a Wire.
    /// Components are just an abstraction. Connected wires are only "injected" into the component,
    /// allowing the internal logic of the component to operate on those signals.
    /// A component port has no inherent width. The internal circuit does.
    class Component
    {
    protected:

        // Components do not have many ports (many of them have less than 10). Thus, a vector
        // search is enough for this use case and reduces the overhead of using a map.
        using PortMap = std::vector<std::pair<std::string, Wire*>>;
        PortMap m_inSignals;
        PortMap m_outSignals;

    public:
        /// Initializer for component ports where each input/output gets a wire assigned.
        using PortInitializer = PortMap;

        explicit Component(const PortInitializer& inPorts, const PortInitializer& outPorts);
        virtual ~Component();

        /// Gets the wire that is currently connected to the specified port, or nullptr if no wire is connected.
        /// @param portName The name of the port to query.
        /// @returns Wire connected, or nullptr if nonexistent.
        /// @throws std::invalid_argument when trying to query a non-existent port.
        /// @note Same as `component[portName]`.
        [[nodiscard]]
        Wire* getPort(const std::string& portName) const;

        /// Overloaded operator[] to access ports by name.
        /// @note Same as getPort().
        [[nodiscard]]
        Wire* operator[](const std::string& portName) const;

        /// Checks if the component has an input port with the specified name.
        /// @param portName The name of the input port to check.
        /// @returns True if the input port exists, false otherwise.
        [[nodiscard]]
        bool hasInputPort(const std::string& portName) const;

        /// @brief Checks if the component has an output port with the specified name.
        /// @param portName The name of the output port to check.
        /// @returns True if the output port exists, false otherwise.
        [[nodiscard]]
        bool hasOutputPort(const std::string& portName) const;

        /// Checks if the component has a port with the specified name.
        /// @param portName The name of the port to check.
        /// @returns True if the port exists, false otherwise.
        [[nodiscard]]
        bool hasPort(const std::string& portName) const;

        /// Updates the component. Can be used by time-aware components such 
        /// as asynchronous circuits or sequential elements
        /// with an specific delay.
        virtual void update() { /* Optional override */ };
    };

} // namespace Pulse::Engine


#endif // PULSE_COMPONENT_H