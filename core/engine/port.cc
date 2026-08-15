#include "port.h"

namespace Pulse
{
    OutputPort::OutputPort(bitWidth_t bitWidth) : ISignalBase(bitWidth), ISignalEmitter(bitWidth) { }

    OutputPort::~OutputPort() { }

    LogicVector OutputPort::peek() const
    {
        return m_state;
    }

    bool OutputPort::drive(const LogicVector& state, ttl_t ttl)
    {
        m_state = state;
        bool ttlExpiredSomewhere = false;
        for (auto* target : m_targets)
        {
            ttlExpiredSomewhere |= target->notify(ttl - 1);
        }
        return ttlExpiredSomewhere;
    }


    // --------------------------------------------------------------------------------------------


    InputPort::InputPort(bitWidth_t bitWidth) : ISignalBase(bitWidth), ISignalReceiver(bitWidth) { }

    InputPort::~InputPort() { }

    bool InputPort::notify(ttl_t ttl)
    {
        if (m_methodInvoker && m_owner) [[likely]]
        {
            m_methodInvoker(m_owner, ttl);
            return true;
        }
        return false;
    }

    LogicVector InputPort::pull() const
    {
        return m_state;
    }

    // --------------------------------------------------------------------------------------------

}