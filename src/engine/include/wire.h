#ifndef PULSE_WIRE_H
#define PULSE_WIRE_H

#include <cstdint>

#include "signalInterface.h"

namespace Pulse::Engine
{
    /// Intermediate node or bus wire connecting components and ports.
    ///
    /// A wire takes its state from the sources connected to it. A wire without sources can instead be driven directly
    /// (drive()), which is how processes assign their signals: the value then stays on the wire until it is driven again.
    /// A wire knows nothing about time: an EventProbe watches it when its 'event is needed.
    class Wire : public ISignalReceiver, public ISignalEmitter
    {
        /// Current state of the signal.
        LogicVector m_state;

        virtual bool onNotify(ttl_t ttl) override;

        /// Stores a new state and notifies the targets when it differs from the current one.
        bool setState(LogicVector newState, ttl_t ttl);

    public:

        /// @param bitWidth Width of the wire.
        /// @param defaultValue State of the wire until a source is connected or the wire is driven. By design, the first
        /// source connected overwrites it.
        explicit Wire(bitWidth_t bitWidth = BITWIDTH_DEFAULT, LogicVector defaultValue = LogicVector::HighZ());
        virtual ~Wire() override;

        /// Returns the logic state of this signal.
        [[nodiscard]]
        virtual LogicVector peek() const override;

        /// Sets the state of the wire directly and propagates it to the targets, as long as no source is connected to
        /// the wire. A wire with sources takes its state from them only, so the value is then ignored.
        /// @param value The logic state to drive (masked to the width of the wire).
        /// @param ttl Optional time-to-live (TTL) value for signal propagation.
        /// @returns false if TTL expired somewhere in the propagation, true otherwise (also when the value is ignored).
        bool drive(LogicVector value, ttl_t ttl = TTL_DEFAULT);
    };
}

#endif // PULSE_WIRE_H
