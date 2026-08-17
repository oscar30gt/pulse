#include "splitter.h"

namespace Pulse
{
    Splitter::Splitter(Wire* in, Wire* out, std::pair<bitWidth_t, bitWidth_t> range)
        : Component({ {"in", in} }, { {"out", out} }),
        m_in(in->width(), this, &Splitter::recalculate),
        m_out((range.first >= range.second) ? (range.first - range.second + 1) : (range.second - range.first + 1)),
        m_range(range)
    {
        if (range.first >= in->width() || range.second >= in->width())
        {
            throw bit_width_mismatch(
                "Splitter: Bit range exceeds input wire width. Input wire width: "
                + std::to_string(in->width()) + ", range: [" + std::to_string(range.first)
                + ", " + std::to_string(range.second) + "]"
            );
        }

        m_in.addSource(in);

        try
        {
            // Out must be able to connect to a port that is the same width as the range of bits being extracted.
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Splitter construction failed: " + std::string(e.what()));
        }
    }

    Splitter::~Splitter() = default;

    bool Splitter::recalculate(ttl_t ttl)
    {
        LogicVector inputState = m_in.pull();
        LogicVector outputState = inputState.range(m_range.first, m_range.second);
        return m_out.drive(outputState, ttl);
    }
}