#include "gates.h"
#include <iostream> // For debug output
namespace Pulse
{
    ANDGate::ANDGate(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &ANDGate::recalculate),
        m_in1(in0->width(), this, &ANDGate::recalculate),       // Width is assumed to be the same as in0
        m_out(in0->width())                                     // Width is assumed to be the same as in0
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

    ANDGate::~ANDGate() = default;

    bool ANDGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();
        return m_out.drive(a & b, ttl);
    }

    // --------------------------------------------------------------------------------------------

    ORGate::ORGate(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &ORGate::recalculate),
        m_in1(in0->width(), this, &ORGate::recalculate),        // Width is assumed to be the same as in0
        m_out(in0->width())                                     // Width is assumed to be the same as in0
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
            throw bit_width_mismatch("OR gate construction failed: " + std::string(e.what()));
        }
    }

    ORGate::~ORGate() = default;

    bool ORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();
        return m_out.drive(a | b, ttl);
    }

    // --------------------------------------------------------------------------------------------

    XORGate::XORGate(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &XORGate::recalculate),
        m_in1(in0->width(), this, &XORGate::recalculate),       // Width is assumed to be the same as in0
        m_out(in0->width())                                     // Width is assumed to be the same as in0
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
            throw bit_width_mismatch("XOR gate construction failed: " + std::string(e.what()));
        }
    }

    XORGate::~XORGate() = default;

    bool XORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();
        return m_out.drive(a ^ b, ttl);
    }

    // --------------------------------------------------------------------------------------------

    NANDGate::NANDGate(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &NANDGate::recalculate),
        m_in1(in0->width(), this, &NANDGate::recalculate),      // Width is assumed to be the same as in0
        m_out(in0->width())                                     // Width is assumed to be the same as in0
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
            throw bit_width_mismatch("NAND gate construction failed: " + std::string(e.what()));
        }
    }

    NANDGate::~NANDGate() = default;

    bool NANDGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();
        return m_out.drive(~(a & b), ttl);
    }

    // --------------------------------------------------------------------------------------------

    NORGate::NORGate(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &NORGate::recalculate),
        m_in1(in0->width(), this, &NORGate::recalculate),       // Width is assumed to be the same as in0
        m_out(in0->width())                                     // Width is assumed to be the same as in0
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
            throw bit_width_mismatch("NOR gate construction failed: " + std::string(e.what()));
        }
    }

    NORGate::~NORGate() = default;

    bool NORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();
        return m_out.drive(~(a | b), ttl);
    }

    // --------------------------------------------------------------------------------------------

    XNORGate::XNORGate(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &XNORGate::recalculate),
        m_in1(in0->width(), this, &XNORGate::recalculate),      // Width is assumed to be the same as in0
        m_out(in0->width())                                     // Width is assumed to be the same as in0
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
            throw bit_width_mismatch("XNOR gate construction failed: " + std::string(e.what()));
        }
    }

    XNORGate::~XNORGate() = default;

    bool XNORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = m_in0.pull();
        LogicVector b = m_in1.pull();
        return m_out.drive(~(a ^ b), ttl);
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