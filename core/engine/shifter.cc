#include "shifter.h"

namespace Pulse
{
    Shifter::Shifter(Wire* in, Wire* shamt, Wire* out, ShiftOperation op)
        : Component({ {"in", in}, {"shamt", shamt} }, { {"out", out} }),
        m_in(in->width(), this, &Shifter::recalculate),
        m_shamt(shamt->width(), this, &Shifter::recalculate),
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

        LogicVector result;
        switch (m_operation)
        {
            case ShiftOperation::LogicalLeft:
                result = input.lsl((uint8_t)shiftAmount, m_in.width());
                break;
            case ShiftOperation::LogicalRight:
                result = input.lsr((uint8_t)shiftAmount, m_in.width());
                break;
            case ShiftOperation::ArithmeticRight:
                result = input.asr((uint8_t)shiftAmount, m_in.width());
                break;
            case ShiftOperation::RotateLeft:
                result = input.rol((uint8_t)shiftAmount, m_in.width());
                break;
            case ShiftOperation::RotateRight:
                result = input.ror((uint8_t)shiftAmount, m_in.width());
                break;
        }

        return m_out.drive(result, ttl);
    }

} // namespace Pulse