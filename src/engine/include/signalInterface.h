#ifndef PULSE_SIGNAL_INTERFACE_H
#define PULSE_SIGNAL_INTERFACE_H

#include <cstdint>
#include <stdexcept>

#include "miniSet.h"
#include "logicVector.h"

namespace Pulse
{
    typedef uint16_t ttl_t;                             /// Time-to-live (TTL) type for signal propagation.
    static constexpr ttl_t TTL_DEFAULT = 512;           /// Default TTL value for signal propagation.

    typedef uint8_t bitWidth_t;                         /// Bit width type for signals and ports.
    static constexpr bitWidth_t BITWIDTH_DEFAULT = 64;  /// Default bit width for signals
    static constexpr bitWidth_t BITWIDTH_MAX = 64;      /// Maximum bit width for signals and ports.
}

namespace Pulse::Engine
{
    // --------------------------------------------------------------------------------------------

    class bit_width_mismatch : public std::invalid_argument
    {
    public:
        bit_width_mismatch(const std::string& message, bitWidth_t expected, bitWidth_t actual);
        bit_width_mismatch(const std::string& message);
    };

    // --------------------------------------------------------------------------------------------

    /// Base interface for all signal elements.
    class ISignalBase
    {
    protected:

        /// Bit width of the signal. Default is 64 bits.
        /// This will prevent signals from being connected to ports of different widths.
        const bitWidth_t m_bitWidth;

    public:

        explicit ISignalBase(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~ISignalBase();

        /// Bit width of the element.
        [[nodiscard]]
        bitWidth_t width() const;
    };

    // --------------------------------------------------------------------------------------------

    /// Interface for elements that can receive a logic state from a signal.
    class ISignalReceiver : virtual public ISignalBase
    {
        friend class ISignalEmitter;

        // Internal functions to add and remove sources without triggering the source port.
        void addSourceInternal(ISignalEmitter* source);
        void removeSourceInternal(ISignalEmitter* source);

        /// Optional overrideable function to handle notifications from connected sources.
        /// @param ttl Time-to-live (TTL) value for the next signal propagation.
        /// @note No need to check for TTL expiration here, as it is implicitly handled in the notify() function.
        /// NEVER decrease TTL manually. Just propagate the same TTL value to the next notify() call, if any.
        virtual bool onNotify(ttl_t ttl) { return true; }

    protected:

        /// Source ports connected to the signal.
        MiniSet<ISignalEmitter*> m_sources;

    public:

        explicit ISignalReceiver(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~ISignalReceiver() override;

        /// Checks if a certain source is connected to this receiver.
        /// @param source The signal emitter to check.
        /// @returns True if the source is connected, false otherwise.
        [[nodiscard]]
        bool hasSource(ISignalEmitter* source) const;

        /// Adds a source (if not already connected) to this receiver.
        /// Bit width of the source and receiver must match. Otherwise, an exception is thrown.
        /// @param source The signal emitter to add.
        void addSource(ISignalEmitter* source);

        /// Removes a source (if any) from this receiver.
        /// @param source The signal emitter to remove.
        void removeSource(ISignalEmitter* source);

        /// Resolves the logic state of the signal based on all connected sources.
        /// @returns The resolved logic state.
        [[nodiscard]]
        LogicVector resolve() const;

        /// Notifies the receiver of a change is some of its sources.
        /// @param ttl Optional time-to-live (TTL) value for signal propagation.
        /// As it is common for signals to recursively notify each other, 
        //// this prevents infinite loops and allows for controlled propagation.
        /// @return false if TTL expired somewhere in the propagation, true otherwise.
        bool notify(ttl_t ttl = TTL_DEFAULT);
    };

    // --------------------------------------------------------------------------------------------

    /// Interface for elements that can emit a logic state to a signal.
    class ISignalEmitter : virtual public ISignalBase
    {
        friend class ISignalReceiver;

        // Internal functions to add and remove targets without triggering the target port.
        void addTargetInternal(ISignalReceiver* target);
        void removeTargetInternal(ISignalReceiver* target);

    protected:

        /// Target ports connected to the signal.
        MiniSet<ISignalReceiver*> m_targets;

    public:

        explicit ISignalEmitter(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~ISignalEmitter() override;

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
        virtual LogicVector peek() const = 0;
    };
}

#endif // PULSE_SIGNAL_INTERFACE_H