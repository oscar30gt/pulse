#ifndef PULSE_GATES_H
#define PULSE_GATES_H

#include <string>
#include <unordered_map>

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse
{
    /// Common interface for 2-input logic gates.
    class ITwoInputGate : public Component
    {
        virtual bool recalculate(ttl_t ttl) = 0;
        virtual void onConnected(const std::string& portName, Wire& signal) override final;
        virtual void onDisconnected(const std::string& portName, Wire& signal) override final;

    protected:
        SignalDrain in0;
        SignalDrain in1;
        SignalSource out;

    public:
        ITwoInputGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~ITwoInputGate() override;
    };

    // --------------------------------------------------------------------------------------------

    /// A simple 2-input AND gate component.
    class ANDGate : public ITwoInputGate
    {
        bool recalculate(ttl_t ttl);

    public:
        ANDGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~ANDGate() override;
    };

    /// A simple 2-input OR gate component.
    class ORGate : public ITwoInputGate
    {
        bool recalculate(ttl_t ttl);

    public:
        ORGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~ORGate() override;
    };

    /// A simple 2-input XOR gate component.
    class XORGate : public ITwoInputGate
    {
        bool recalculate(ttl_t ttl);

    public:
        XORGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~XORGate() override;
    };

    /// A simple 2-input NAND gate component.
    class NANDGate : public ITwoInputGate
    {
        bool recalculate(ttl_t ttl);

    public:
        NANDGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~NANDGate() override;
    };

    /// A simple 2-input NOR gate component.
    class NORGate : public ITwoInputGate
    {
        bool recalculate(ttl_t ttl);

    public:
        NORGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~NORGate() override;
    };

    /// A simple 2-input XNOR gate component.
    class XNORGate : public ITwoInputGate
    {
        bool recalculate(ttl_t ttl);

    public:
        XNORGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~XNORGate() override;
    };

    // --------------------------------------------------------------------------------------------

    /// A simple NOT gate component.
    class NOTGate : public Component
    {
        SignalDrain in;
        SignalSource out;

        virtual void onConnected(const std::string& portName, Wire& signal) override final;
        virtual void onDisconnected(const std::string& portName, Wire& signal) override final;

        bool recalculate(ttl_t ttl);

    public:
        NOTGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~NOTGate() override;
    };

} // namespace Pulse


#endif // PULSE_GATES_H