#ifndef PULSE_SPLITTER_H
#define PULSE_SPLITTER_H

#include <string>
#include <unordered_map>

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    /// Receives a single input signal and extracts a certain range of bits from it.
    /// Range must be within the bounds of the input signal's bit width.
    /// Inputs: "in" (any bits)
    /// Outputs: "out" (same bits as range)
    class Splitter : public Component
    {
        SignalDrain m_in;
        SignalSource m_out;
        std::pair<bitWidth_t, bitWidth_t> m_range;

        bool recalculate(ttl_t ttl);

    public:
        explicit Splitter(Wire* in, Wire* out, std::pair<bitWidth_t, bitWidth_t> range);
        virtual ~Splitter() override final;
    };

} // namespace Pulse::Engine

#endif // PULSE_SPLITTER_H