#include "component.h"

#include <stdexcept>
#include <unordered_set>

namespace Pulse
{
    Component::Component(std::initializer_list<std::string> inPorts, std::initializer_list<std::string> outPorts)
    {
        std::unordered_set<std::string> uniquePorts;
        uniquePorts.reserve(inPorts.size() + outPorts.size());

        // Validate and register input ports
        for (const auto& port : inPorts)
        {
            if (!uniquePorts.insert(port).second)
            {
                throw std::invalid_argument("Duplicate port name detected in Component constructor: '" + port + "'");
            }
            m_inSignals[port] = nullptr;
        }

        // Validate and register output ports.
        for (const auto& port : outPorts)
        {
            if (!uniquePorts.insert(port).second)
            {
                throw std::invalid_argument("Duplicate port name detected in Component constructor: '" + port + "'");
            }
            m_outSignals[port] = nullptr;
        }
    }

    Component::~Component() = default;

    void Component::connect(const std::string& signalName, Wire& signal)
    {
        if (auto it = m_inSignals.find(signalName); it != m_inSignals.end())
        {
            if (it->second != &signal) [[likely]]
            {
                it->second = &signal;
                onConnected(signalName, signal);
            }
        }
        else if (auto itOut = m_outSignals.find(signalName); itOut != m_outSignals.end())
        {
            if (itOut->second != &signal) [[likely]]
            {
                itOut->second = &signal;
                onConnected(signalName, signal);
            }
        }
        else
        {
            throw std::invalid_argument("Cannot connect signal to non-existent port: " + signalName);
        }
    }

    void Component::disconnect(const std::string& signalName)
    {
        if (auto it = m_inSignals.find(signalName); it != m_inSignals.end())
        {
            if (it->second != nullptr) [[likely]]
            {
                auto oldSignal = it->second;
                it->second = nullptr;
                onDisconnected(signalName, *oldSignal);
            }
        }
        else if (auto itOut = m_outSignals.find(signalName); itOut != m_outSignals.end())
        {
            if (itOut->second != nullptr) [[likely]]
            {
                auto oldSignal = itOut->second;
                itOut->second = nullptr;
                onDisconnected(signalName, *oldSignal);
            }
        }
        else
        {
            throw std::invalid_argument("Cannot disconnect signal from non-existent port: " + signalName);
        }
    }

    Wire* Component::getSignal(const std::string& signalName) const
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