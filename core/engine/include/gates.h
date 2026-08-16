#ifndef PULSE_GATES_H
#define PULSE_GATES_H

#include <string>
#include <unordered_map>

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{

    /// A simple 2-input AND gate component.
    /// Inputs: "in0" (X bits), "in1" (X bits)
    /// Outputs: "out" (X bits)
    class ANDGate : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        ANDGate(Wire* in0, Wire* in1, Wire* out);
        ~ANDGate();
    };

    // --------------------------------------------------------------------------------------------

    /// A simple 2-input OR gate component.
    /// Inputs: "in0" (X bits), "in1" (X bits)
    /// Outputs: "out" (X bits)
    class ORGate : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        ORGate(Wire* in0, Wire* in1, Wire* out);
        ~ORGate();
    };

    // --------------------------------------------------------------------------------------------

    /// A simple 2-input XOR gate component.
    /// Inputs: "in0" (X bits), "in1" (X bits)
    /// Outputs: "out" (X bits)
    class XORGate : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        XORGate(Wire* in0, Wire* in1, Wire* out);
        ~XORGate();
    };

    /// A simple 2-input NAND gate component.
    /// Inputs: "in0" (X bits), "in1" (X bits)
    /// Outputs: "out" (X bits)
    class NANDGate : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        NANDGate(Wire* in0, Wire* in1, Wire* out);
        ~NANDGate();
    };

    // --------------------------------------------------------------------------------------------

    /// A simple 2-input NOR gate component.
    /// Inputs: "in0" (X bits), "in1" (X bits)
    /// Outputs: "out" (X bits)
    class NORGate : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        NORGate(Wire* in0, Wire* in1, Wire* out);
        ~NORGate();
    };

    /// A simple 2-input XNOR gate component.
    /// Inputs: "in0" (X bits), "in1" (X bits)
    /// Outputs: "out" (X bits)
    class XNORGate : public Component
    {
        SignalDrain m_in0;
        SignalDrain m_in1;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        XNORGate(Wire* in0, Wire* in1, Wire* out);
        ~XNORGate();
    };

    // --------------------------------------------------------------------------------------------

    /// A simple NOT gate component.
    /// Inputs: "in" (X bits)
    /// Outputs: "out" (X bits)
    class NOTGate : public Component
    {
        SignalDrain m_in;
        SignalSource m_out;

        bool recalculate(ttl_t ttl);

    public:
        NOTGate(Wire* in, Wire* out);
        ~NOTGate();
    };

} // namespace Pulse


#endif // PULSE_GATES_H