#ifndef PULSE_GATES_H
#define PULSE_GATES_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// Available gate operations a binary gate can perform.
    enum class BinaryOp : uint8_t
    {
        AND,        // And (&)
        OR,         // Or (|)
        XOR,        // Exclusive Or (^)
        NAND,       // Not And
        NOR,        // Not Or
        XNOR        // Exclusive Not Or
    };

    /// A simple 2-input gate component.
    /// Inputs: "in0" (N bits), "in1" (N bits)
    /// Outputs: "out" (N bits)
    class BinaryGate : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;
        BinaryOp m_operation : 3;

        bool recalculate(ttl_t ttl);

    public:
        BinaryGate(Wire* in0, Wire* in1, Wire* out, BinaryOp op = BinaryOp::AND);
        ~BinaryGate();
    };

    // --------------------------------------------------------------------------------------------

    /// A simple NOT gate component.
    /// Inputs: "in" (N bits)
    /// Outputs: "out" (N bits)
    class NOTGate : public Component
    {
        SignalDrain m_in;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        NOTGate(Wire* in, Wire* out);
        ~NOTGate();
    };

} // namespace Pulse


#endif // PULSE_GATES_H