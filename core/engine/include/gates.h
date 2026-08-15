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
    class ANDGate : public Component
    {
        SignalDrain in0;
        SignalDrain in1;
        SignalSource out;

        bool recalculate(ttl_t ttl);
        void onConnected(const std::string& portName, Wire& signal) override;
        void onDisconnected(const std::string& portName, Wire& signal) override;

    public:
        ANDGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~ANDGate() override;
    };

} // namespace Pulse


#endif // PULSE_GATES_H