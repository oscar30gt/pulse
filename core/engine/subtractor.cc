#include "subtractor.h"

namespace Pulse::Engine
{
    Subtractor::Subtractor(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &Subtractor::recalculate),
        m_in1(in0->width(), this, &Subtractor::recalculate),        // Must have the same width as in0
        m_out(in0->width())                                         // Must have the same width as in0
    {
        try
        {
            m_in0.addSource(in0);
            m_in1.addSource(in1);
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Subtractor construction failed: " + std::string(e.what()));
        }
    }

    Subtractor::~Subtractor() = default;

    bool Subtractor::recalculate(ttl_t ttl)
    {
        LogicVector result = m_in0.pull() - m_in1.pull();
        return m_out.drive(result, ttl);
    }
} // namespace Pulse::Engine