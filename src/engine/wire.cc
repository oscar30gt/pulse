#include "wire.h"
#include <stdexcept>

namespace Pulse::Engine
{
    Wire::Wire(bitWidth_t bitWidth, LogicVector defaultValue)
        : ISignalBase(bitWidth), ISignalReceiver(bitWidth), ISignalEmitter(bitWidth), m_state(defaultValue.range(bitWidth))
    { }

    Wire::~Wire() { }

    LogicVector Wire::peek() const
    {
        return m_state;
    }

    bool Wire::setState(LogicVector newState, ttl_t ttl)
    {
        if (newState == m_state) return true;
        m_state = newState;

        // Notify all target ports connected to this signal
        bool allOk = true;
        for (ISignalReceiver* target : m_targets)
        {
            allOk &= target->notify(ttl);
        }

        return allOk;
    }

    bool Wire::onNotify(ttl_t ttl)
    {
        return setState(resolve(), ttl); // Update the signal state based on connected sources
    }

    bool Wire::drive(LogicVector value, ttl_t ttl)
    {
        if (!m_sources.empty()) return true; // The sources own the state of the wire
        return setState(value.range(m_bitWidth), ttl);
    }
}
