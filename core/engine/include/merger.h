#ifndef PULSE_MERGER_H
#define PULSE_MERGER_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    /// Receives two input signals and concatenates them into a single output signal.
    /// Result is high&low (high is the most significant bits, low is the least significant bits).
    /// Inputs: "low" (any bits), "high" (any bits)
    /// Outputs: "out" (low + high bits)
    class Merger : public Component
    {
        SignalDrain m_low;
        SignalDrain m_high;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        explicit Merger(Wire* low, Wire* high, Wire* out);
        virtual ~Merger() override final;
    };

} // namespace Pulse::Engine


#endif // PULSE_MERGER_H