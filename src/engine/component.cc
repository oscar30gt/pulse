#include "component.h"

#include <stdexcept>
#include <unordered_set>

namespace Pulse::Engine
{
    Component::Component(const PortInitializer& inPorts, const PortInitializer& outPorts)
    {
        std::unordered_set<std::string> uniquePorts;
        uniquePorts.reserve(inPorts.size() + outPorts.size());

        // Validate and register input ports
        for (const auto& port : inPorts)
        {
            if (!uniquePorts.insert(port.first).second)
            {
                throw std::invalid_argument("Duplicate port name detected in Component constructor: '" + port.first + "'");
            }
            m_inSignals.push_back(std::move(port));
        }

        // Validate and register output ports.
        for (const auto& port : outPorts)
        {
            if (!uniquePorts.insert(port.first).second)
            {
                throw std::invalid_argument("Duplicate port name detected in Component constructor: '" + port.first + "'");
            }
            m_outSignals.push_back(std::move(port));
        }
    }

    Component::~Component() = default;

    Wire* Component::getPort(const std::string& signalName) const
    {
        for (const auto& port : m_inSignals) if (port.first == signalName)
        {
            return port.second;
        }

        for (const auto& port : m_outSignals) if (port.first == signalName)
        {
            return port.second;
        }

        throw std::invalid_argument("Cannot get signal from non-existent port: " + signalName);
    }

    Wire* Component::operator[](const std::string& signalName) const
    {
        return getPort(signalName);
    }

    bool Component::hasInputPort(const std::string& signalName) const
    {
        for (const auto& port : m_inSignals) if (port.first == signalName)
        {
            return true;
        }
        
        return false;
    }

    bool Component::hasOutputPort(const std::string& signalName) const
    {
        for (const auto& port : m_outSignals) if (port.first == signalName)
        {
            return true;
        }

        return false;
    }

    bool Component::hasPort(const std::string& signalName) const
    {
        return hasInputPort(signalName) || hasOutputPort(signalName);
    }
}