#ifndef PULSE_WIRE_H
#define PULSE_WIRE_H

#include <cstdint>

#include "signalInterface.h"

namespace Pulse
{
    /// Intermediate node or bus wire connecting components and ports.
    class Wire : public ISignalReceiver, public ISignalEmitter
    {
        /// Current state of the signal.
        LogicVector m_state;

        virtual bool onNotify(ttl_t ttl) override;

    public:

        explicit Wire(bitWidth_t bitWidth = BITWIDTH_DEFAULT);
        virtual ~Wire() override;

        /// Returns the logic state of this signal. 
        [[nodiscard]]
        virtual LogicVector peek() const override;
    };
}

#endif // PULSE_WIRE_H