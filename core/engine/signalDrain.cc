#include "signalDrain.h"
#include <iostream> // For debug output
namespace Pulse
{
    SignalDrain::SignalDrain(bitWidth_t bitWidth) : ISignalBase(bitWidth), ISignalReceiver(bitWidth), m_state(LogicVector::HighZ()) { }

    SignalDrain::~SignalDrain() { }

    bool SignalDrain::onNotify(ttl_t ttl)
    {
        m_state = resolve();
        if (m_methodInvoker && m_owner) [[likely]]
        {
            return m_methodInvoker(m_owner, ttl);
        }
        return true; // ttl is not expired, but no method to invoke.
    }

    LogicVector SignalDrain::pull() const
    {
        return m_state;
    }
}