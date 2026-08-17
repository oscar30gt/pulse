#include "component.h"

#include <stdexcept>
#include <unordered_set>

namespace Pulse::Engine
{
    Component::Component(PortInitializer inPorts, PortInitializer outPorts)
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
            m_inSignals[port.first] = port.second;
        }

        // Validate and register output ports.
        for (const auto& port : outPorts)
        {
            if (!uniquePorts.insert(port.first).second)
            {
                throw std::invalid_argument("Duplicate port name detected in Component constructor: '" + port.first + "'");
            }
            m_outSignals[port.first] = port.second;
        }
    }

    Component::~Component() = default;

    Wire* Component::getPort(const std::string& signalName) const
    {
        if (auto it = m_inSignals.find(signalName); it != m_inSignals.end())
        {
            return it->second;
        }
        else if (auto itOut = m_outSignals.find(signalName); itOut != m_outSignals.end())
        {
            return itOut->second;
        }
        else
        {
            throw std::invalid_argument("Cannot get signal from non-existent port: " + signalName);
        }
    }

    Wire* Component::operator[](const std::string& signalName) const
    {
        return getPort(signalName);
    }

    bool Component::hasInputPort(const std::string& signalName) const
    {
        return m_inSignals.find(signalName) != m_inSignals.end();
    }

    bool Component::hasOutputPort(const std::string& signalName) const
    {
        return m_outSignals.find(signalName) != m_outSignals.end();
    }

    bool Component::hasPort(const std::string& signalName) const
    {
        return hasInputPort(signalName) || hasOutputPort(signalName);
    }
}