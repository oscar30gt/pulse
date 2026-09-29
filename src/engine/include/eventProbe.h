#ifndef PULSE_EVENT_PROBE_H
#define PULSE_EVENT_PROBE_H

#include <optional>

#include "component.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    /// Exposes the VHDL 'event of a wire as a logic signal: the only component that knows about simulation ticks.
    ///
    /// At the start of every tick the probe latches its input: there is an event when the input differs from the value it
    /// had at the start of the previous tick, that is, when it changed during the previous tick. The first latch never
    /// reports one (VHDL initialization). Every probe of a design latches before any of them publishes, so a probe output
    /// that changes another probed wire counts at the next tick (Subgraph does so).
    /// Its output is what combinational logic reads for `s'event`, and what process boxes run on (their triggers).
    /// Inputs: "in" (any bits)
    /// Outputs: "out" (1 bit): '1' during a tick where the input has an event, '0' otherwise
    class EventProbe : public Component
    {
        Wire* m_in;
        SignalSource m_out;
        /// The input at the previous latch; empty before the first one.
        std::optional<LogicVector> m_previous;
        /// The event found by the last latch.
        bool m_event = false;

    public:
        EventProbe(Wire* in, Wire* out);
        virtual ~EventProbe() override;

        /// Step 1 of a tick: compares the input with its value at the previous latch. Only reads the input.
        void latch();

        /// Step 2 of a tick: drives the output with the event found by latch().
        void publish();
    };

} // namespace Pulse::Engine

#endif // PULSE_EVENT_PROBE_H
