#ifndef PULSE_ENGINE_SIGNALS_H
#define PULSE_ENGINE_SIGNALS_H

#include <cstdint>
#include "miniSet.h"

/// Wires and signals for connecting components and ports in a circuit simulation.
/// Elements DO NOT manage memory. Pointers are just used for referencing other elements.
/// Higher level objects are responsible for providing those elements and deleting them when
/// actually needed.

namespace Pulse::Engine
{
    typedef uint16_t ttl_t;                     /// Time-to-live (TTL) type for signal propagation.
    static constexpr ttl_t TTL_DEFAULT = 512;   /// Default TTL value for signal propagation.

    /// Logic state of a signal.
    enum class LogicState : uint8_t
    {
        Low             = '0',  /// Logic 0 (0V)
        High            = '1',  /// Logic 1 (Vdd)
        HighZ           = 'Z',  /// High Impedance (floating)
        Unknown         = 'X',  /// Unknown (Error)
        Uninitialized   = 'U',  /// Uninitialized (Initial state)
    };

    // --------------------------------------------------------------------------------------------

    /// Interface for elements that can receive a logic state from a signal.
    class ISignalReceiver
    {
        friend class ISignalEmitter;

        // Internal functions to add and remove sources without triggering the source port.
        void addSourceInternal(ISignalEmitter* source);
        void removeSourceInternal(ISignalEmitter* source);

    protected:
        /// Source ports connected to the signal.
        MiniSet<ISignalEmitter*> m_sources;

    public:
        ISignalReceiver();
        virtual ~ISignalReceiver();

        /// Checks if a certain source is connected to this receiver.
        /// @param source The signal emitter to check.
        /// @returns True if the source is connected, false otherwise.
        [[nodiscard]]
        bool hasSource(ISignalEmitter* source) const;

        /// Adds a source (if not already connected) to this receiver.
        /// @param source The signal emitter to add.
        /// @note Establishes a bidirectional connection so it is not 
        /// necessary to call addTarget on the source.
        void addSource(ISignalEmitter* source);

        /// Removes a source (if any) from this receiver.
        /// @param source The signal emitter to remove.
        /// @note Connection is broken in both directions, so it is not 
        /// necessary to call removeTarget on the source.
        void removeSource(ISignalEmitter* source);

        /// Resolves the logic state of the signal based on all connected sources.
        /// @returns The resolved logic state.
        [[nodiscard]]
        LogicState resolve() const;

        /// Notifies the receiver of a change is some of its sources.
        /// @param ttl Optional time-to-live (TTL) value for signal propagation.
        /// As it is common for signals to recursively notify each other, 
        //// this prevents infinite loops and allows for controlled propagation.
        /// @return True if TTL expired somewhere in the propagation, false otherwise.
        virtual bool notify(ttl_t ttl = TTL_DEFAULT) = 0;
    };

    /// Interface for elements that can emit a logic state to a signal.
    class ISignalEmitter
    {
        friend class ISignalReceiver;

        // Internal functions to add and remove targets without triggering the target port.
        void addTargetInternal(ISignalReceiver* target);
        void removeTargetInternal(ISignalReceiver* target);

    protected:
        /// Target ports connected to the signal.
        MiniSet<ISignalReceiver*> m_targets;

    public:
        ISignalEmitter();
        virtual ~ISignalEmitter();

        /// Checks if a certain target is connected to this emitter.
        /// @param target The signal receiver to check.
        /// @return True if the target is connected, false otherwise.
        [[nodiscard]]
        bool hasTarget(ISignalReceiver* target) const;
        
        /// Adds a target (if not already connected) to this emitter.
        /// @param target The signal receiver to add.
        /// @note Establishes a bidirectional connection so it is not
        /// necessary to call addSource on the target.
        void addTarget(ISignalReceiver* target);

        /// Removes a target (if any) from this emitter.
        /// @param target The signal receiver to remove.
        /// @note Connection is broken in both directions, so it is not
        /// necessary to call removeSource on the target.
        void removeTarget(ISignalReceiver* target);

        /// Reads the current logic state of the emitted signal.
        /// @return Logic state of the signal emitter.
        [[nodiscard]]
        virtual LogicState read() const = 0;
    };

    // --------------------------------------------------------------------------------------------

    /// Intermediate node or bus wire connecting components and ports.
    class Signal : public ISignalReceiver, public ISignalEmitter
    {
        LogicState m_state = LogicState::HighZ; /// Current state of the signal.

    public:
        Signal();
        virtual ~Signal();

        /// Returns the logic state of this signal. 
        [[nodiscard]]
        virtual LogicState read() const override;
        
        /// Notifies this signal of a change in some of its sources, 
        /// updating its state and propagating the change if any.
        /// @param ttl Optional time-to-live (TTL) value for signal propagation.
        /// @return True if TTL expired somewhere in the propagation, false otherwise.
        virtual bool notify(ttl_t ttl = TTL_DEFAULT) override;
    };
}

#endif // PULSE_ENGINE_SIGNALS_H