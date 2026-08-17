#ifndef PULSE_COMPARATOR_H
#define PULSE_COMPARATOR_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// Available comparator operations a binary comparator can perform.
    enum class CompareOp : uint8_t
    {
        Equals,                 // Equal (==)
        NotEquals,              // Not equal (!=)
        LessThan,               // Less than (<)
        LessThanEqual,          // Less than or equal (<=)
        GreaterThan,            // Greater than (>)
        GreaterThanEqual,       // Greater than or equal (>=)
    };

    /// Available comparison modes for signed and unsigned comparisons.
    enum class CompareMode : uint8_t
    {
        Signed,                 // Signed comparison
        Unsigned,               // Unsigned comparison
    };

    /// A binary comparator component that compares two input signals and outputs a single-bit result.
    /// The comparison operation and mode (signed/unsigned) can be specified.
    /// Inputs: "in0" (N bits), "in1" (N bits)
    /// Outputs: "out" (1 bit)
    class Comparator : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;
        CompareOp m_operation : 3;
        CompareMode m_mode : 1;

        bool recalculate(ttl_t ttl);

    public:
        Comparator(
            Wire* in0, Wire* in1, Wire* out,
            CompareOp op = CompareOp::Equals,
            CompareMode mode = CompareMode::Unsigned
        );
        virtual ~Comparator() override;
    };

} // namespace Pulse

#endif // PULSE_COMPARATOR_H