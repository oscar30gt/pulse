#ifndef PULSE_SIGNAL_SOURCE_H
#define PULSE_SIGNAL_SOURCE_H

#include <functional>
#include "wire.h"

namespace Pulse
{
    /// Signal sources are used to drive a logic state onto a signal.
    class SignalSource : public ISignalEmitter
    {
        /// Internal state of the output port.
        LogicVector m_state;

    public:
        explicit SignalSource(bitWidth_t bitWidth = 64);
        virtual ~SignalSource() override;

        /// Returns the logic state this port is currently outputting.
        [[nodiscard]]
        virtual LogicVector peek() const override;

        /// Defines the logic state this port will output to connected signals
        /// and propagates the change through the signal network.
        /// @param state The logic state to drive onto the signal.
        /// @param ttl Optional time-to-live (TTL) value for signal propagation.
        /// @returns false if TTL expired somewhere in the propagation, true otherwise.
        bool drive(LogicVector state, ttl_t ttl = TTL_DEFAULT);
    };
}

#endif // PULSE_SIGNAL_SOURCE_H