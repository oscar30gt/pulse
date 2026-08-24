#include "controlledBuffer.h"

namespace Pulse::Engine
{
    ControlledBuffer::ControlledBuffer(Wire* in, Wire* enable, Wire* out)
        : Component({ {"in", in}, {"enable", enable} }, { {"out", out} }),
        m_in(in->width(), this, &ControlledBuffer::recalculate),
        m_enable(1, this, &ControlledBuffer::recalculate),
        m_out(in->width())
    {
        try
        {
            m_in.addSource(in);
            m_enable.addSource(enable);
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Controlled buffer construction failed: " + std::string(e.what()));
        }
    }

    ControlledBuffer::~ControlledBuffer() = default;

    bool ControlledBuffer::recalculate(ttl_t ttl)
    {
        if (m_enable.pull())
        {
            return m_out.drive(m_in.pull(), ttl);
        }
        else
        {
            return m_out.drive(LogicVector::HighZ().range(m_out.width()), ttl);
        }
    }

} // namespace Pulse::Engine