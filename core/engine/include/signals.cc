#include "signals.h"

namespace Pulse::Engine
{
    // -------- OutputPort ------------------------------------------------------------------------

    OutputPort::OutputPort() = default;
    OutputPort::~OutputPort()
    {
        disconnect();
    };

    State OutputPort::read() const
    {
        return m_state;
    }

    void OutputPort::drive(State newState)
    {
        if (m_state == newState) return;

        m_state = newState;
        if (m_target)
        {
            m_target->update();
        }
    }

    // -------- InputPort -------------------------------------------------------------------------


    InputPort::InputPort() = default;
    InputPort::~InputPort()
    {
        disconnect();
    };

    void InputPort::update(uint16_t)
    {
        if (!m_source) return;

        State newState = m_source->read();
        if (newState != m_state)
        {
            m_state = newState;
            if (m_onChange)
            {
                m_onChange(m_state);
            }
        }
    }

    State InputPort::value() const
    {
        return m_state;
    }

    // -------- Signal ----------------------------------------------------------------------------

    Signal::Signal() = default;
    Signal::~Signal()
    {
        for (auto source : m_sources)
        {
            disconnectSource(source);
        }
        for (auto target : m_targets)
        {
            disconnectTarget(target);
        }
    }

    State Signal::read() const
    {
        return m_state;
    }

    void Signal::update(uint16_t ttl)
    {
        if (ttl == 0) return;

        State newState = State::HighZ;
        for (auto source : m_sources)
        {
            State sourceState = source->read();

            // Error is always propagated
            if (sourceState == State::Unknown)
            {
                newState = State::Unknown;
                break;
            }

            // Other states are only propagated if the signal is not already defined
            // or if they all agree on the same value.
            else if (sourceState != State::HighZ)
            {
                if (newState == State::HighZ)
                {
                    newState = sourceState;
                }
                else if (newState != sourceState)
                {
                    newState = State::Unknown;
                    break;
                }
            }

            // HighZ is ignored
        }

        if (newState != m_state)
        {
            m_state = newState;
            for (auto target : m_targets)
            {
                target->update(ttl - 1);
            }
        }
    }
}