#ifndef PULSE_SHIFTER_H
#define PULSE_SHIFTER_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// Enumerates the available shift operations for the Shifter component.
    enum class ShiftOperation : uint8_t
    {
        LogicalLeft,               /// Logical left shift
        LogicalRight,              /// Logical right shift
        ArithmeticRight,    /// Arithmetic right shift (preserves sign bit)
        RotateLeft,         /// Rotate left
        RotateRight         /// Rotate right
    };

    /// Barrel shifter component that shifts a signal the specified number of bits.
    /// Available shift operations: left, right, arithmetic right, rotate left, rotate right.
    /// Inputs: "in" (X bits), "shamt" (6 bits)
    /// Outputs: "out" (X bits)
    class Shifter : public Component
    {
        SignalDrain m_in;
        SignalDrain m_shamt;
        SignalSource m_out;
        ShiftOperation m_operation : 3;

        bool recalculate(ttl_t ttl);

    public:
        explicit Shifter(Wire* in, Wire* shamt, Wire* out, ShiftOperation op = ShiftOperation::LogicalLeft);
        virtual ~Shifter() override final;
    };

} // namespace Pulse


#endif // PULSE_SHIFTER_H