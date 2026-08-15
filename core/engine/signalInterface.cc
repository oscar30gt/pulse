#include "signalInterface.h"

#include <stdexcept>
#include <iostream> // For debug output
namespace Pulse
{
    // -------- Base ------------------------------------------------------------------------------


    ISignalBase::ISignalBase(bitWidth_t bitWidth) : m_bitWidth(bitWidth) { }

    ISignalBase::~ISignalBase() { }

    bitWidth_t ISignalBase::width() const
    {
        return m_bitWidth;
    }


    // -------- Receiver --------------------------------------------------------------------------


    ISignalReceiver::ISignalReceiver(bitWidth_t bitWidth) : ISignalBase(bitWidth) { }

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
        if (source && source->width() != m_bitWidth)
            throw std::invalid_argument("Bit width mismatch between emitter and receiver.");

        if (m_sources.insert(source)) {
            source->addTargetInternal(this);
            this->notify();
        }
    }

    void ISignalReceiver::removeSource(ISignalEmitter* source)
    {
        if (source && m_sources.erase(source)) {
            source->removeTargetInternal(this);
            this->notify();
        }
    }

    LogicVector ISignalReceiver::resolve() const
    {
        LogicVector resolvedState;
        for (auto source : m_sources)
            resolvedState = resolvedState.resolve(source->peek());

        return resolvedState;
    }

    bool ISignalReceiver::notify(ttl_t ttl)
    {
        if (ttl == 0) return false; // TTL expired, stop propagation
        return onNotify(ttl - 1);
    }


    // -------- Emitter ---------------------------------------------------------------------------


    ISignalEmitter::ISignalEmitter(bitWidth_t bitWidth) : ISignalBase(bitWidth) { }

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
        if (target->width() != m_bitWidth)
            throw std::invalid_argument("Bit width mismatch between emitter and receiver.");

        if (m_targets.insert(target)) {
            target->addSourceInternal(this);
            target->notify();
        }
    }

    void ISignalEmitter::removeTarget(ISignalReceiver* target)
    {
        if (m_targets.erase(target)) {
            target->removeSourceInternal(this);
            target->notify();
        }
    }
}