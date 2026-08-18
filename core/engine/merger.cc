#include "merger.h"

namespace Pulse::Engine
{
    Merger::Merger(Wire* low, Wire* high, Wire* out)
        : Component({ {"low", low}, {"high", high} }, { {"out", out} }),
        m_low(low->width(), this, &Merger::recalculate),
        m_high(high->width(), this, &Merger::recalculate),
        m_out(low->width() + high->width())
    {
        m_low.addSource(low);
        m_high.addSource(high);

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
        LogicVector lessSignificant = m_low.pull();
        LogicVector moreSignificant = m_high.pull();
        LogicVector result = lessSignificant | moreSignificant.lsl(m_low.width());
        return m_out.drive(result, ttl);
    }
}