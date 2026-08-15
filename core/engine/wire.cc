#include "wire.h"

#include <stdexcept>

namespace Pulse
{
    Wire::Wire(bitWidth_t bitWidth) : ISignalBase(bitWidth), ISignalReceiver(bitWidth), ISignalEmitter(bitWidth), m_state(LogicVector::HighZ()) { }

    Wire::~Wire() { }

    LogicVector Wire::peek() const
    {
        return m_state;
    }

    bool Wire::onNotify(ttl_t ttl)
    {
        LogicVector newState = resolve(); // Update the signal state based on connected sources
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
}