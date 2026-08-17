#include "merger.h"

namespace Pulse::Engine
{
    Merger::Merger(Wire* in0, Wire* in1, Wire* out)
        : Component({ {"in0", in0}, {"in1", in1} }, { {"out", out} }),
        m_in0(in0->width(), this, &Merger::recalculate),
        m_in1(in1->width(), this, &Merger::recalculate),
        m_out(in0->width() + in1->width())
    {
        m_in0.addSource(in0);
        m_in1.addSource(in1);

        try
        {
            // Out must be able to connect to a port that is the same width as the sum of the input widths.
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Merger construction failed: " + std::string(e.what()));
        }
    }

    Merger::~Merger() = default;

    bool Merger::recalculate(ttl_t ttl)
    {
        LogicVector lessSignificant = m_in0.pull();
        LogicVector moreSignificant = m_in1.pull();
        LogicVector result = lessSignificant | moreSignificant.lsl(m_in0.width());
        return m_out.drive(result, ttl);
    }
}