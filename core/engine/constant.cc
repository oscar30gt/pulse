#include "constant.h"

#include <stdexcept>

namespace Pulse
{
    Constant::Constant(LogicVector state, bitWidth_t bitWidth) : ISignalBase(bitWidth), ISignalEmitter(bitWidth), m_state(state) { }

    Constant::~Constant() { }

    LogicVector Constant::peek() const
    {
        return m_state;
    }
}