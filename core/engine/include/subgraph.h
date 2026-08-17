#ifndef PULSE_SUBGRAPH_H
#define PULSE_SUBGRAPH_H

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"
#include "blueprint.h"

namespace Pulse::Engine
{
    /// A dynamically generated subgraph component defined by a blueprint object.
    class Subgraph : public Component
    {
        using Blueprint = Pulse::Parser::Blueprint;

    public:
        Subgraph(const Blueprint& bp, PortInitializer inPorts, PortInitializer outPorts);
        virtual ~Subgraph() override;
    };

} // namespace Pulse::Engine

#endif // PULSE_SUBGRAPH_H