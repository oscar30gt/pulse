#include "constant.h"

#include <stdexcept>

namespace Pulse
{
    Constant::Constant(LogicVector state, bitWidth_t bitWidth) : ISignalBase(bitWidth), ISignalEmitter(bitWidth), m_state(state.range(bitWidth)) { }

    Constant::~Constant() = default;

    LogicVector Constant::peek() const
    {
        return m_state;
    }
}