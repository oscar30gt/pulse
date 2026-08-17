#ifndef PULSE_ADDER_H
#define PULSE_ADDER_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// A binary adder that performs addition on two input signals and produces an output signal.
    /// Inputs: "in0" (N bits), "in1" (N bits)
    /// Outputs: "out" (N bits)
    class Adder : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        Adder(Wire* in0, Wire* in1, Wire* out);
        virtual ~Adder() override;
    };

} // namespace Pulse

#endif // PULSE_ADDER_H