#include "clock.h"

namespace Pulse::Engine
{
    Clock::Clock(Wire* out, bool initiallyHigh, uint64_t duration)
        :Clock(out, initiallyHigh, duration, duration)
    { }

    Clock::Clock(Wire* out, bool initiallyHigh, uint64_t highDuration, uint64_t lowDuration)
        : Component({}, { {"out", out} }),
        m_out(1),
        m_highDuration(highDuration),
        m_lowDuration(lowDuration),
        m_countdown(initiallyHigh ? highDuration : lowDuration)
    {
        try
        {
            m_out.addTarget(out);
        }
        catch (const bit_width_mismatch&)
        {
            throw bit_width_mismatch("Clock output wire width is not 1", 1, out->width());
        }

        m_out.drive(LogicVector::FromBool(initiallyHigh));
    }

    Clock::~Clock() { }

    void Clock::update()
    {
        m_countdown--;
        if (m_countdown == 0)
        {
            bool currentState = (bool)m_out.peek();
            m_out.drive(LogicVector::FromBool(!currentState));
            m_countdown = currentState ? m_lowDuration : m_highDuration;
        }
    }
}