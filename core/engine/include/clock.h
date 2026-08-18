#ifndef PULSE_CLOCK_H
#define PULSE_CLOCK_H

#include <cstdint>

#include "component.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    /// A clock component that generates a periodic signal on its output port.
    /// The clock can be configured with different high and low durations, 
    /// or a single duration for both high and low states.
    /// Inputs: None
    /// Outputs: "out" (1 bit)
    class Clock : public Component
    {
        SignalSource m_out;
        uint64_t m_highDuration;
        uint64_t m_lowDuration;
        uint64_t m_countdown;

    public:
        explicit Clock(Wire* out, bool initiallyHigh = false, uint64_t duration = 1);
        explicit Clock(Wire* out, bool initiallyHigh, uint64_t highDuration, uint64_t lowDuration);
        virtual ~Clock() override;

        /// Advances time by one tick, updating the output signal based on the configured durations.
        virtual void update() override;
    };
}

#endif // PULSE_CLOCK_H