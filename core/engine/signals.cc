#include "signals.h"

#include <stdexcept>

namespace Pulse
{
    // --------------------------------------------------------------------------------------------

    ISignalElement::ISignalElement(bitWidth_t bitWidth) : m_bitWidth(bitWidth) { }

    ISignalElement::~ISignalElement() { }

    bitWidth_t ISignalElement::width() const
    {
        return m_bitWidth;
    }

    // --------------------------------------------------------------------------------------------


    ISignalReceiver::ISignalReceiver(bitWidth_t bitWidth) : ISignalElement(bitWidth) { }

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
        if (source && source->m_bitWidth != m_bitWidth)
            throw std::invalid_argument("Bit width mismatch between emitter and receiver.");

        if (m_sources.insert(source))
            source->addTargetInternal(this);
    }

    void ISignalReceiver::removeSource(ISignalEmitter* source)
    {
        if (source && m_sources.erase(source))
            source->removeTargetInternal(this);
    }

    LogicVector ISignalReceiver::resolve() const
    {
        LogicVector resolvedState;
        for (auto source : m_sources)
            resolvedState = resolvedState.resolve(source->read());

        return resolvedState;
    }


    // -------- InputPort -------------------------------------------------------------------------


    ISignalEmitter::ISignalEmitter(bitWidth_t bitWidth) : ISignalElement(bitWidth) { }

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
        if (target && target->m_bitWidth != m_bitWidth)
            throw std::invalid_argument("Bit width mismatch between emitter and receiver.");

        if (m_targets.insert(target))
            target->addSourceInternal(this);
    }

    void ISignalEmitter::removeTarget(ISignalReceiver* target)
    {
        if (m_targets.erase(target))
            target->removeSourceInternal(this);
    }


    // -------- Signal ----------------------------------------------------------------------------


    Signal::Signal(bitWidth_t bitWidth) : ISignalElement(bitWidth), ISignalReceiver(bitWidth), ISignalEmitter(bitWidth) { }

    Signal::~Signal() { }

    LogicVector Signal::read() const
    {
        return m_state;
    }

    bool Signal::notify(ttl_t ttl)
    {
        if (ttl == 0) return true; // TTL expired, stop propagation

        LogicVector newState = resolve(); // Update the signal state based on connected sources
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