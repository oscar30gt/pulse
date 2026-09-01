#ifndef PULSE_CONTROLLED_BUFFER_H
#define PULSE_CONTROLLED_BUFFER_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    /// A controlled buffer that lets a signal pass through to the output when the enable signal is high, 
    /// and outputs high-impedance (Z) when the enable signal is low.
    /// Inputs: "in" (N bits), "enable" (1 bit)
    /// Outputs: "out" (N bits)
    class ControlledBuffer : public Component
    {
        SignalDrain m_in;
        SignalDrain m_enable;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        ControlledBuffer(Wire* in, Wire* enable, Wire* out);
        virtual ~ControlledBuffer() override;
    };

} // namespace Pulse::Engine

#endif // PULSE_CONTROLLED_BUFFER_H