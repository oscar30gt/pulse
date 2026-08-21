#include "shifter.h"
#include <iostream>

namespace Pulse::Engine
{
    Shifter::Shifter(Wire* in, Wire* shamt, Wire* out, ShiftOp op)
        : Component({ {"in", in}, {"shamt", shamt} }, { {"out", out} }),
        m_in(in->width(), this, &Shifter::recalculate),
        m_shamt(6, this, &Shifter::recalculate),
        m_out(in->width()),  // Output must match the input width
        m_operation(op)
    {
        try
        {
            m_in.addSource(in);
            m_shamt.addSource(shamt);
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Shifter construction failed: " + std::string(e.what()));
        }
    }

    Shifter::~Shifter() = default;

    bool Shifter::recalculate(ttl_t ttl)
    {
        LogicVector input = m_in.pull();
        LogicVector shiftAmount = m_shamt.pull();

        if (!shiftAmount.isDefinite()) {
            return m_out.drive(LogicVector::Unknown(), ttl);
        }

        LogicVector result;
        switch (m_operation)
        {
            case ShiftOp::LogicalLeft:
                result = input.lsl(static_cast<uint8_t>(shiftAmount), m_in.width());
                break;
            case ShiftOp::LogicalRight:
                result = input.lsr(static_cast<uint8_t>(shiftAmount), m_in.width());
                break;
            case ShiftOp::ArithmeticRight:
                result = input.asr(static_cast<uint8_t>(shiftAmount), m_in.width());
                break;
            case ShiftOp::RotateLeft:
                result = input.rol(static_cast<uint8_t>(shiftAmount), m_in.width());
                break;
            case ShiftOp::RotateRight:
                result = input.ror(static_cast<uint8_t>(shiftAmount), m_in.width());
                break;
        }

        return m_out.drive(result, ttl);
    }

} // namespace Pulse::Engine