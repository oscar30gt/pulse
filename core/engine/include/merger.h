#ifndef PULSE_MERGER_H
#define PULSE_MERGER_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// Receives two input signals and concatenates them into a single output signal.
    /// Result is in1&in0 (in1 is the most significant bits, in0 is the least significant bits).
    /// Inputs: "in0" (any bits), "in1" (any bits)
    /// Outputs: "out" (in0 + in1 bits)
    class Merger : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        explicit Merger(Wire* in0, Wire* in1, Wire* out);
        virtual ~Merger() override final;
    };

} // namespace Pulse


#endif // PULSE_MERGER_H