#include "signalSource.h"

namespace Pulse
{
    SignalSource::SignalSource(bitWidth_t bitWidth) : ISignalBase(bitWidth), ISignalEmitter(bitWidth), m_state(LogicVector::HighZ()) { }

    SignalSource::~SignalSource() = default;

    LogicVector SignalSource::peek() const
    {
        return m_state;
    }

    bool SignalSource::drive(LogicVector state, ttl_t ttl)
    {
        m_state = state.range(m_bitWidth);
        bool allOk = true;
        for (auto* target : m_targets)
        {
            allOk &= target->notify(ttl);
        }
        return allOk;
    }
}