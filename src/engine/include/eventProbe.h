#ifndef PULSE_EVENT_PROBE_H
#define PULSE_EVENT_PROBE_H

#include "component.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    /// Exposes the 'event flag of a wire as a logic signal, so combinational logic can use it (VHDL `s'event`).
    /// The flag only changes when the watched wire is updated at the start of a tick, so the probe must be updated
    /// right after the wires and before any other component (Subgraph does so).
    /// Inputs: "in" (any bits)
    /// Outputs: "out" (1 bit): '1' while the input wire has an event, '0' otherwise
    class EventProbe : public Component
    {
        Wire* m_in;
        SignalSource m_out;

    public:
        EventProbe(Wire* in, Wire* out);
        virtual ~EventProbe() override;

        /// Drives the output with the current 'event flag of the input wire.
        virtual void update() override;
    };

} // namespace Pulse::Engine

#endif // PULSE_EVENT_PROBE_H
