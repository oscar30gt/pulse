#include "comparator.h"

namespace Pulse
{
    Comparator::Comparator(Wire* in0, Wire* in1, Wire* out, CompareOp op, CompareMode mode)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &Comparator::recalculate),
        m_in1(in0->width(), this, &Comparator::recalculate), // in1 must have the same width as in0
        m_out(1),
        m_operation(op),
        m_mode(mode)
    {
        try
        {
            m_in0.addSource(in0);
            m_in1.addSource(in1);
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Comparator construction failed: " + std::string(e.what()));
        }
    }

    Comparator::~Comparator() = default;

    bool Comparator::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();

        if (!a.isDefinite() || !b.isDefinite()) [[unlikely]]
        {
            return m_out.drive(LogicVector::Unknown(), ttl);
        }

        bool result = false;
        uint64_t a_val = static_cast<uint64_t>(a);
        uint64_t b_val = static_cast<uint64_t>(b);

        // Add sign extension for signed comparisons if the inputs are negative
        if (m_mode == CompareMode::Signed)
        {
            uint64_t ones = ~0ULL << m_in0.width();
            a_val = a.bit(m_in0.width() - 1) == '1' ? (a_val | ones) : a_val;
            b_val = b.bit(m_in1.width() - 1) == '1' ? (b_val | ones) : b_val;

            int64_t a_signed = static_cast<int64_t>(a_val);
            int64_t b_signed = static_cast<int64_t>(b_val);

            switch (m_operation)
            {
                case CompareOp::Equals:           result = (a_signed == b_signed); break;
                case CompareOp::NotEquals:        result = (a_signed != b_signed); break;
                case CompareOp::LessThan:         result = (a_signed < b_signed);  break;
                case CompareOp::LessThanEqual:    result = (a_signed <= b_signed); break;
                case CompareOp::GreaterThan:      result = (a_signed > b_signed);  break;
                case CompareOp::GreaterThanEqual: result = (a_signed >= b_signed); break;
            }
        }

        else switch (m_operation)
        {
            case CompareOp::Equals:           result = (a_val == b_val); break;
            case CompareOp::NotEquals:        result = (a_val != b_val); break;
            case CompareOp::LessThan:         result = (a_val < b_val);  break;
            case CompareOp::LessThanEqual:    result = (a_val <= b_val); break;
            case CompareOp::GreaterThan:      result = (a_val > b_val);  break;
            case CompareOp::GreaterThanEqual: result = (a_val >= b_val); break;
        }

        return m_out.drive(LogicVector::FromBool(result), ttl);
    }

} // namespace Pulse
