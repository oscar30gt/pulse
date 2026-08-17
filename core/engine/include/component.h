#ifndef PULSE_COMPONENT_H
#define PULSE_COMPONENT_H

#include <string>
#include <span>
#include <unordered_map>

#include "wire.h"

namespace Pulse::Engine
{
    class Component
    {
        using PortMap = std::unordered_map<std::string, Wire*>;
        PortMap m_inSignals;
        PortMap m_outSignals;

        using PortInitializer = std::vector<std::pair<std::string, Wire*>>;

    public:
        explicit Component(PortInitializer inPorts, PortInitializer outPorts);
        virtual ~Component();

        /// Gets the wire that is currently connected to the specified port, or nullptr if no wire is connected.
        /// @param portName The name of the port to query.
        /// @returns Wire connected, or nullptr if nonexistent.
        /// @throws std::invalid_argument when trying to query a non-existent port.
        /// @note Same as `component[portName]`.
        [[nodiscard]]
        Wire* getPort(const std::string& portName) const;

        /// Overloaded operator[] to access ports by name.
        /// Same as getPort().
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
    };

} // namespace Pulse::Engine


#endif // PULSE_COMPONENT_H