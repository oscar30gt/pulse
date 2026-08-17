#ifndef PULSE_SUBSTRACTOR_H
#define PULSE_SUBSTRACTOR_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// A binary subtractor that performs subtraction on two input signals and produces an output signal.
    /// Inputs: "in0" (N bits), "in1" (N bits)
    /// Outputs: "out" (N bits)
    class Subtractor : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        Subtractor(Wire* in0, Wire* in1, Wire* out);
        virtual ~Subtractor() override;
    };

} // namespace Pulse

#endif // PULSE_SUBSTRACTOR_H