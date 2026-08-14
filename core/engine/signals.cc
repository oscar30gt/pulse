#include "signals.h"

namespace Pulse::Engine
{
    // -------- OutputPort ------------------------------------------------------------------------


    ISignalReceiver::ISignalReceiver() { }

    ISignalReceiver::~ISignalReceiver()
    {
        while (!m_sources.empty())
            (*m_sources.begin())->removeTarget(this);
    }

    void ISignalReceiver::addSourceInternal(ISignalEmitter* source)
    {
        m_sources.insert(source);
    }

    void ISignalReceiver::removeSourceInternal(ISignalEmitter* source)
    {
        m_sources.erase(source);
    }

    bool ISignalReceiver::hasSource(ISignalEmitter* source) const
    {
        return m_sources.contains(source);
    }

    void ISignalReceiver::addSource(ISignalEmitter* source)
    {
        if (m_sources.insert(source))
            source->addTargetInternal(this);
    }

    void ISignalReceiver::removeSource(ISignalEmitter* source)
    {
        if (m_sources.erase(source))
            source->removeTargetInternal(this);
    }

    LogicState ISignalReceiver::resolve() const
    {
        LogicState resolvedState = LogicState::HighZ;
        for (auto source : m_sources)
        {
            LogicState sourceState = source->read();

            if (sourceState == LogicState::HighZ)
                continue; // High Impedance are just open connections that don't affect the resolved state.

            if (resolvedState == LogicState::HighZ)
            {
                resolvedState = sourceState;
                continue;
            }

            if (sourceState != resolvedState)
                return LogicState::Unknown; // Conflict between sources.
        }

        return resolvedState;
    }


    // -------- InputPort -------------------------------------------------------------------------


    ISignalEmitter::ISignalEmitter() { }

    ISignalEmitter::~ISignalEmitter()
    {
        while (!m_targets.empty())
            (*m_targets.begin())->removeSource(this);
    }

    void ISignalEmitter::addTargetInternal(ISignalReceiver* target)
    {
        m_targets.insert(target);
    }

    void ISignalEmitter::removeTargetInternal(ISignalReceiver* target)
    {
        m_targets.erase(target);
    }

    bool ISignalEmitter::hasTarget(ISignalReceiver* target) const
    {
        return m_targets.contains(target);
    }

    void ISignalEmitter::addTarget(ISignalReceiver* target)
    {
        if (m_targets.insert(target))
            target->addSourceInternal(this);
    }

    void ISignalEmitter::removeTarget(ISignalReceiver* target)
    {
        if (m_targets.erase(target))
            target->removeSourceInternal(this);
    }


    // -------- Signal ----------------------------------------------------------------------------


    Signal::Signal() : ISignalReceiver(), ISignalEmitter() { }

    Signal::~Signal() { }

    LogicState Signal::read() const
    {
        return m_state;
    }

    bool Signal::notify(ttl_t ttl)
    {
        if (ttl == 0) return true; // TTL expired, stop propagation

        LogicState newState = resolve(); // Update the signal state based on connected sources
        if (newState == m_state) return false;
        m_state = newState;

        // Notify all target ports connected to this signal
        bool ttlExpiredSomewhere = false;
        for (ISignalReceiver* target : m_targets)
        {
            if (target != nullptr)
                ttlExpiredSomewhere |= target->notify(ttl - 1);
        }

        return ttlExpiredSomewhere;
    }
}