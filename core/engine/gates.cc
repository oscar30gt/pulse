#include "gates.h"

namespace Pulse
{
    ITwoInputGate::ITwoInputGate(bitWidth_t bitWidth)
        : Component({ "in0", "in1" }, { "out" }),
        in0(bitWidth, this, &ITwoInputGate::recalculate),
        in1(bitWidth, this, &ITwoInputGate::recalculate),
        out(bitWidth)
    { }

    ITwoInputGate::~ITwoInputGate() = default;

    void ITwoInputGate::onConnected(const std::string& portName, Wire& signal)
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

    void ITwoInputGate::onDisconnected(const std::string& portName, Wire& signal)
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

    // --------------------------------------------------------------------------------------------

    ANDGate::ANDGate(bitWidth_t bitWidth) : ITwoInputGate(bitWidth) { }
    ANDGate::~ANDGate() = default;

    bool ANDGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in0.pull();
        LogicVector b = in1.pull();
        return out.drive(a & b, ttl);
    }

    // --------------------------------------------------------------------------------------------

    ORGate::ORGate(bitWidth_t bitWidth) : ITwoInputGate(bitWidth) { }
    ORGate::~ORGate() = default;

    bool ORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in0.pull();
        LogicVector b = in1.pull();
        return out.drive(a | b, ttl);
    }

    // --------------------------------------------------------------------------------------------

    XORGate::XORGate(bitWidth_t bitWidth) : ITwoInputGate(bitWidth) { }
    XORGate::~XORGate() = default;

    bool XORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in0.pull();
        LogicVector b = in1.pull();
        return out.drive(a ^ b, ttl);
    }

    // --------------------------------------------------------------------------------------------

    NANDGate::NANDGate(bitWidth_t bitWidth) : ITwoInputGate(bitWidth) { }
    NANDGate::~NANDGate() = default;

    bool NANDGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in0.pull();
        LogicVector b = in1.pull();
        return out.drive(~(a & b), ttl);
    }

    // --------------------------------------------------------------------------------------------

    NORGate::NORGate(bitWidth_t bitWidth) : ITwoInputGate(bitWidth) { }
    NORGate::~NORGate() = default;

    bool NORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in0.pull();
        LogicVector b = in1.pull();
        return out.drive(~(a | b), ttl);
    }

    // --------------------------------------------------------------------------------------------

    XNORGate::XNORGate(bitWidth_t bitWidth) : ITwoInputGate(bitWidth) { }
    XNORGate::~XNORGate() = default;

    bool XNORGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in0.pull();
        LogicVector b = in1.pull();
        return out.drive(~(a ^ b), ttl);
    }

    // --------------------------------------------------------------------------------------------

    NOTGate::NOTGate(bitWidth_t bitWidth)
        : Component({ "in" }, { "out" }),
        in(bitWidth, this, &NOTGate::recalculate),
        out(bitWidth)
    { }

    NOTGate::~NOTGate() = default;

    void NOTGate::onConnected(const std::string& portName, Wire& signal)
    {
        if (portName == "in")
        {
            in.addSource(&signal);
        }
        else if (portName == "out")
        {
            out.addTarget(&signal);
        }
    }

    void NOTGate::onDisconnected(const std::string& portName, Wire& signal)
    {
        if (portName == "in")
        {
            in.removeSource(&signal);
        }
        else if (portName == "out")
        {
            out.removeTarget(&signal);
        }
    }

    bool NOTGate::recalculate(ttl_t ttl)
    {
        LogicVector a = in.pull();
        return out.drive(~a, ttl);
    }
}