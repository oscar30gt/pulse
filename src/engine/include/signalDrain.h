#ifndef PULSE_SIGNAL_DRAIN_H
#define PULSE_SIGNAL_DRAIN_H

#include <functional>

#include "wire.h"

namespace Pulse::Engine
{
    /// Input ports are used to pull a logic state from a signal.
    class SignalDrain : public ISignalReceiver
    {
        /// Internal state of the input port.
        LogicVector m_state;

        using MethodInvokerFn = std::function<bool(void* owner, ttl_t ttl)>;
        void* m_owner = nullptr;
        MethodInvokerFn m_methodInvoker;

        virtual bool onNotify(ttl_t ttl) override;

    public:
        explicit SignalDrain(bitWidth_t bitWidth = BITWIDTH_DEFAULT);

        template <typename T>
        explicit SignalDrain(bitWidth_t bitWidth, T* owner, bool (T::* method)(ttl_t));
        virtual ~SignalDrain() override;

        /// Reads the logic state currently being input to this port.
        /// @return The logic state of the input port.
        [[nodiscard]]
        LogicVector pull() const;
    };

    template <typename T>
    SignalDrain::SignalDrain(bitWidth_t bitWidth, T* owner, bool (T::* method)(ttl_t))
        : ISignalBase(bitWidth), ISignalReceiver(bitWidth), m_owner(owner)
    {
        if (owner && method)
        {
            // Common-signature lambda to invoke the member function on the owner object.
            m_methodInvoker = [method](void* ownerPtr, ttl_t ttl) -> bool
            {
                return (static_cast<T*>(ownerPtr)->*method)(ttl);
            };
        }
    }
}

#endif // PULSE_SIGNAL_DRAIN_H