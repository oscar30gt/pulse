#include "gates.h"

namespace Pulse
{
    ANDGate::ANDGate(bitWidth_t bitWidth)
        : Component({ "in0", "in1" }, { "out" }),
        in0(bitWidth, this, &ANDGate::recalculate),
        in1(bitWidth, this, &ANDGate::recalculate),
        out(bitWidth)
    { }

    ANDGate::~ANDGate() = default;

    bool ANDGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in0.pull();
        LogicVector b = in1.pull();
        return out.drive(a & b, ttl);
    }

    void ANDGate::onConnected(const std::string& portName, Wire& signal)
    {
        if (portName == "in0")
        {
            in0.addSource(&signal);
        }
        else if (portName == "in1")
        {
            in1.addSource(&signal);
        }
        else if (portName == "out")
        {
            out.addTarget(&signal);
        }
    }

    void ANDGate::onDisconnected(const std::string& portName, Wire& signal)
    {
        if (portName == "in0")
        {
            in0.removeSource(&signal);
        }
        else if (portName == "in1")
        {
            in1.removeSource(&signal);
        }
        else if (portName == "out")
        {
            out.removeTarget(&signal);
        }
    }
}