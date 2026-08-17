#include "gates.h"

namespace Pulse
{
    BinaryGate::BinaryGate(Wire* in0, Wire* in1, Wire* out, BinaryOp op)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &BinaryGate::recalculate),
        m_in1(in0->width(), this, &BinaryGate::recalculate),   // Width is assumed to be the same as in0
        m_out(in0->width()),                                   // Width is assumed to be the same as in0
        m_operation(op)
    {
        m_in0.addSource(in0);

        try
        {
            // As all port widths are forced to be the same. If any of the widths are mismatched,
            // connection will throw a bit_width_mismatch exception.
            m_in1.addSource(in1);
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("AND gate construction failed: " + std::string(e.what()));
        }
    }

    BinaryGate::~BinaryGate() = default;

    bool BinaryGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();

        LogicVector result;
        switch (m_operation)
        {
            case BinaryOp::AND:
                result = a & b;
                break;
            case BinaryOp::OR:
                result = a | b;
                break;
            case BinaryOp::XOR:
                result = a ^ b;
                break;
            case BinaryOp::NAND:
                result = ~(a & b);
                break;
            case BinaryOp::NOR:
                result = ~(a | b);
                break;
            case BinaryOp::XNOR:
                result = ~(a ^ b);
                break;
        }



        return m_out.drive(result, ttl);
    }

    // --------------------------------------------------------------------------------------------

    NOTGate::NOTGate(Wire* in, Wire* out)
        : Component({ { "in", in } }, { { "out", out } }),
        m_in(in->width(), this, &NOTGate::recalculate),
        m_out(in->width())                                      // Width is assumed to be the same as in
    {
        m_in.addSource(in);

        try
        {
            // As all port widths are forced to be the same. If any of the widths are mismatched,
            // connection will throw a bit_width_mismatch exception.
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("NOT gate construction failed: " + std::string(e.what()));
        }
    }

    NOTGate::~NOTGate() = default;

    bool NOTGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in.pull();
        return m_out.drive(~a, ttl);
    }
}