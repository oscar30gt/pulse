#ifndef PULSE_PORT_H
#define PULSE_PORT_H

#include <concepts>
#include <functional>
#include "signals.h"
#include "logicVector.h"

namespace Pulse
{
    // --------------------------------------------------------------------------------------------

    /// Output ports are used to drive a logic state onto a signal.
    class OutputPort : public ISignalEmitter
    {
        /// Internal state of the output port.
        LogicVector m_state;

    public:
        explicit OutputPort(bitWidth_t bitWidth = 64);
        virtual ~OutputPort() override;

        /// Returns the logic state this port is currently outputting.
        [[nodiscard]]
        virtual LogicVector peek() const override;

        /// Defines the logic state this port will output to connected signals
        /// and propagates the change through the signal network.
        /// @param state The logic state to drive onto the signal.
        /// @param ttl Optional time-to-live (TTL) value for signal propagation.
        /// @returns True if TTL expired somewhere in the propagation, false otherwise.
        bool drive(const LogicVector& state, ttl_t ttl = TTL_DEFAULT);
    };
    
    // --------------------------------------------------------------------------------------------

    /// Concept to ensure a callable type with a ttl_t parameter.
    template <typename F>
    concept PortCallback = std::invocable<F, ttl_t>;

    /// Input ports are used to pull a logic state from a signal.
    class InputPort : public ISignalReceiver
    {
        /// Internal state of the input port.
        LogicVector m_state;

        using MethodInvokerFn = std::function<void(void* owner, ttl_t ttl)>;        
        void* m_owner = nullptr;
        MethodInvokerFn m_methodInvoker;

    public:
        explicit InputPort(bitWidth_t bitWidth = BITWIDTH_DEFAULT);

        template <typename T>
        explicit InputPort(bitWidth_t bitWidth, T* owner, void (T::*method)(ttl_t));
        virtual ~InputPort() override;

        /// @brief Notifies the input port of a input signal change.
        /// @param ttl The time-to-live value for the signal change propagation, if applicable.
        /// @return false if ttl expired somewhere in the propagation, true otherwise.
        virtual bool notify(ttl_t ttl = TTL_DEFAULT) override;

        /// Reads the logic state currently being input to this port.
        /// @return The logic state of the input port.
        [[nodiscard]]
        LogicVector pull() const;
    };

    template <typename T>
    InputPort::InputPort(bitWidth_t bitWidth, T* owner, void (T::*method)(ttl_t))
        : ISignalBase(bitWidth), ISignalReceiver(bitWidth), m_owner(owner)
    {
        if (owner && method)
        {
            // Common-signature lambda to invoke the member function on the owner object.
            m_methodInvoker = [method](void* ownerPtr, ttl_t ttl)
            {
                (static_cast<T*>(ownerPtr)->*method)(ttl);
            };
        }
    }
}

#endif // PULSE_PORT_H