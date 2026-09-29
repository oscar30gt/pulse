#include "eventProbe.h"

namespace Pulse::Engine
{
    EventProbe::EventProbe(Wire* in, Wire* out)
        : Component({ {"in", in} }, { {"out", out} }),
        m_in(in),
        m_out(1)
    {
        m_out.drive(LogicVector::FromBool(false));

        try
        {
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch& e)
        {
            throw bit_width_mismatch("Event probe construction failed: " + std::string(e.what()));
        }
    }

    EventProbe::~EventProbe() = default;

    void EventProbe::latch()
    {
        const LogicVector current = m_in->peek();
        m_event = m_previous.has_value() && *m_previous != current;
        m_previous = current;
    }

    void EventProbe::publish()
    {
        const LogicVector flag = LogicVector::FromBool(m_event);
        if (flag != m_out.peek())
            m_out.drive(flag);
    }

} // namespace Pulse::Engine
