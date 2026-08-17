#ifndef PULSE_CONSTANT_H
#define PULSE_CONSTANT_H

#include <cstdint>

#include "component.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    /// A constant emmitter that always outputs the same logic state.
    /// Inputs: None
    /// Outputs: "out" (N bits)
    class Constant : public Component
    {
        SignalSource m_out;

    public:
        explicit Constant(Wire* out, LogicVector state);
        virtual ~Constant() override;
    };
}

#endif // PULSE_CONSTANT_H