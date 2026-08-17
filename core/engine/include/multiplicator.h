#ifndef PULSE_MULTIPLICATOR_H
#define PULSE_MULTIPLICATOR_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// A binary multiplicator that performs multiplication on two input signals and produces an output signal.
    /// Inputs: "in0" (N bits), "in1" (M bits)
    /// Outputs: "out" (N + M bits)
    class Multiplicator : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        Multiplicator(Wire* in0, Wire* in1, Wire* out);
        virtual ~Multiplicator() override;
    };

} // namespace Pulse

#endif // PULSE_MULTIPLICATOR_H