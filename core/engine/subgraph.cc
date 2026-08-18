#include "subgraph.h"
#include "blueprint.h"

#include "gates.h"
#include "shifter.h"
#include "comparator.h"
#include "splitter.h"
#include "merger.h"
#include "adder.h"
#include "subtractor.h"
#include "multiplicator.h"
#include "controlledBuffer.h"

namespace Pulse::Engine
{
    Subgraph::Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts)
        : Component(inPorts, outPorts)
    {

        for (auto& [name, wire] : bp.wires)
        {
            wires.insert({ name, std::make_unique<Wire>(wire.width) });
        }

        using namespace Pulse::Parser;
        for (auto& [name, component] : bp.components)
        {
            switch (component->type)
            {
                case InstanceType::BinaryGate:
                {
                    auto* gate = static_cast<BinaryGateInstance*>(component.get());
                    auto* in0 = findWire(gate->in0);
                    auto* in1 = findWire(gate->in1);
                    auto* out = findWire(gate->out);
                    components.insert({ name, std::make_unique<BinaryGate>(in0, in1, out, gate->op) });
                }
                break;

                case InstanceType::NotGate:
                {
                    auto* notGate = static_cast<NotGateInstance*>(component.get());
                    auto* in = findWire(notGate->in);
                    auto* out = findWire(notGate->out);
                    components.insert({ name, std::make_unique<NOTGate>(in, out) });
                }

                case InstanceType::Shifter:
                {
                    auto* shifter = static_cast<ShifterInstance*>(component.get());
                    auto* in = findWire(shifter->in);
                    auto* shamt = findWire(shifter->shamt);
                    auto* out = findWire(shifter->out);
                    components.insert({ name, std::make_unique<Shifter>(in, out, shamt, shifter->op) });
                }
                break;

                case InstanceType::Comparator:
                {
                    auto* comparator = static_cast<ComparatorInstance*>(component.get());
                    auto* in0 = findWire(comparator->in0);
                    auto* in1 = findWire(comparator->in1);
                    auto* out = findWire(comparator->out);
                    components.insert({ name, std::make_unique<Comparator>(in0, in1, out, comparator->op, comparator->mode) });
                }
                break;

                case InstanceType::Splitter:
                {
                    auto* splitter = static_cast<SplitterInstance*>(component.get());
                    auto* in = findWire(splitter->in);
                    auto* out = findWire(splitter->out);
                    components.insert({ name, std::make_unique<Splitter>(in, out, std::pair(splitter->high, splitter->low)) });
                }
                break;

                case InstanceType::Merger:
                {
                    auto* merger = static_cast<MergerInstance*>(component.get());
                    auto* low = findWire(merger->low);
                    auto* high = findWire(merger->high);
                    auto* out = findWire(merger->out);
                    components.insert({ name, std::make_unique<Merger>(low, high, out) });
                }
                break;

                case InstanceType::Adder:
                {
                    auto* adder = static_cast<AdderInstance*>(component.get());
                    auto* in0 = findWire(adder->in0);
                    auto* in1 = findWire(adder->in1);
                    auto* out = findWire(adder->out);
                    components.insert({ name, std::make_unique<Adder>(in0, in1, out) });
                }
                break;

                case InstanceType::Subtractor:
                {
                    auto* subtractor = static_cast<SubtractorInstance*>(component.get());
                    auto* in0 = findWire(subtractor->in0);
                    auto* in1 = findWire(subtractor->in1);
                    auto* out = findWire(subtractor->out);
                    components.insert({ name, std::make_unique<Subtractor>(in0, in1, out) });
                }
                break;

                case InstanceType::Multiplicator:
                {
                    auto* multiplicator = static_cast<MultiplicatorInstance*>(component.get());
                    auto* in0 = findWire(multiplicator->in0);
                    auto* in1 = findWire(multiplicator->in1);
                    auto* out = findWire(multiplicator->out);
                    components.insert({ name, std::make_unique<Multiplicator>(in0, in1, out) });
                }
                break;

                case InstanceType::ControlledBuffer:
                {
                    auto* buffer = static_cast<ControlledBufferInstance*>(component.get());
                    auto* in = findWire(buffer->in);
                    auto* enable = findWire(buffer->enable);
                    auto* out = findWire(buffer->out);
                    components.insert({ name, std::make_unique<ControlledBuffer>(in, enable, out) });
                }
                break;

                case InstanceType::Subgraph:
                {
                    auto* subgraph = static_cast<SubgraphInstance*>(component.get());
                    auto* bp = subgraph->bp;
                    auto& portMap = subgraph->portMap;

                    // Map the subgraph's ports to the parent subgraph's wires
                    PortInitializer inPorts, outPorts;

                    for (const auto& portName: subgraph->bp->inPorts)
                    {
                        auto it = portMap.find(portName);
                        if (it == portMap.end())
                            continue; // No signal is connected to this port, skip it.

                        Wire* parentWire = findWire(it->second);
                        inPorts.emplace_back(portName, parentWire);
                    }

                    for (const auto& portName: subgraph->bp->outPorts)
                    {
                        auto it = portMap.find(portName);
                        if (it == portMap.end())
                            continue; // No signal is connected to this port, skip it.

                        Wire* parentWire = findWire(it->second);
                        outPorts.emplace_back(portName, parentWire);
                    }
                    
                    components.insert({ name, std::make_unique<Subgraph>(*bp, inPorts, outPorts) });
                }
                break;

                default:
                    throw std::runtime_error("Subgraph construction failed: Unknown component type.");
            }
        }
    }

    Subgraph::~Subgraph() = default;

    Wire* Subgraph::findWire(const std::string& name)
    {
        // Check if the wire is an input or output port of the subgraph
        if (hasPort(name))
        {
            return getPort(name);
        }

        // Check if the wire is an internal signal of the subgraph
        auto it = wires.find(name);
        if (it != wires.end())
        {
            return it->second.get();
        }

        return nullptr;
    }

    Component* Subgraph::findComponent(const std::string& name)
    {
        auto it = components.find(name);
        if (it != components.end())
        {
            return it->second.get();
        }

        return nullptr;
    }
}