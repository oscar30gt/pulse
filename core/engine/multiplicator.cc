#include "multiplicator.h"

namespace Pulse
{
    Multiplicator::Multiplicator(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &Multiplicator::recalculate),
        m_in1(in1->width(), this, &Multiplicator::recalculate),
        m_out(in0->width() + in1->width())                          // Must be N + M bits
    {
        try
        {
            m_in0.addSource(in0);
            m_in1.addSource(in1);
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Multiplicator construction failed: " + std::string(e.what()));
        }
    }

    Multiplicator::~Multiplicator() = default;

    bool Multiplicator::recalculate(ttl_t ttl)
    {
        LogicVector result = m_in0.pull() * m_in1.pull();
        return m_out.drive(result, ttl);
    }
} // namespace Pulse