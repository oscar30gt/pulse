#ifndef PULSE_COMPONENT_H
#define PULSE_COMPONENT_H

#include <string>
#include <unordered_map>

#include "wire.h"

namespace Pulse
{
    class Component
    {
        std::unordered_map<std::string, Wire*> m_inSignals;
        std::unordered_map<std::string, Wire*> m_outSignals;

    protected:

        // Optional callbacks invoked when a port is connected/disconnected from a wire.
        virtual void onConnected(const std::string& portName, Wire& signal) { /* Optional override */ }
        virtual void onDisconnected(const std::string& portName, Wire& signal) { /* Optional override */ }

    public:
        explicit Component(std::initializer_list<std::string> inPorts, std::initializer_list<std::string> outPorts);
        virtual ~Component();

        /// Connects one of the component's ports to a wire.
        /// @param portName The name of the port to connect.
        /// @param signal The wire to connect to the port.
        /// @throws std::invalid_argument when trying to connect to a non-existent port.
        void connect(const std::string& portName, Wire& signal);

        /// Disconnects one of the component's ports from its connected wire, if any.
        /// @param portName The name of the port to disconnect.
        /// @throws std::invalid_argument when trying to disconnect a non-existent port.
        void disconnect(const std::string& portName);

        /// Gets the wire that is currently connected to the specified port, or nullptr if no wire is connected.
        /// @param portName The name of the port to query.
        /// @returns Wire connected, or nullptr if nonexistent.
        /// @throws std::invalid_argument when trying to query a non-existent port.
        [[nodiscard]]
        Wire* getSignal(const std::string& portName) const;

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

} // namespace Pulse


#endif // PULSE_COMPONENT_H