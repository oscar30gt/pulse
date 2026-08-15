#ifndef PULSE_CONSTANT_H
#define PULSE_CONSTANT_H

#include <cstdint>

#include "signalInterface.h"

namespace Pulse
{
    /// A constant emmitter that always outputs the same logic state.
    class Constant : public ISignalEmitter
    {
        /// Constant state 
        const LogicVector m_state;

    public:
        explicit Constant(LogicVector state, bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~Constant() override;

        /// Returns the logic state of this constant.
        [[nodiscard]]
        virtual LogicVector peek() const override;
    };
}

#endif // PULSE_CONSTANT_H