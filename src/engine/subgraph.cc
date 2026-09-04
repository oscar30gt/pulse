#include "subgraph.h"

#include <stdexcept>

#include "blueprint.h"
#include "gates.h"
#include "shifter.h"
#include "comparator.h"
#include "splitter.h"
#include "concatenator.h"
#include "adder.h"
#include "subtractor.h"
#include "multiplicator.h"
#include "controlledBuffer.h"
#include "constant.h"

namespace Pulse::Engine
{
    Subgraph::Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts)
        : Component(inPorts, outPorts)
    {
        std::vector<std::string> visitedSubgraphs;
        build(bp, visitedSubgraphs);
    }

    Subgraph::Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts, std::vector<std::string>& visitedSubgraphs)
        : Component(inPorts, outPorts)
    {
        build(bp, visitedSubgraphs);
    }

    void Subgraph::build(const Blueprint& bp, std::vector<std::string>& visitedSubgraphs)
    {
        // Wires must be created before components, as components need 
        // to connect to the wires during their construction.
        for (auto& [name, wire] : bp.wires)
        {
            wires.insert({ name, std::make_unique<Wire>(wire.width) });
        }

        // Now, create the components based on the blueprint. 
        // Each component will connect to the appropriate wires, 
        // which can be either internal wires or the subgraph's ports.
        for (auto& [name, component] : bp.components)
        {
            // Instantiate the component based on its type. Subgraph will take ownership of the created component.
            switch (component->type)
            {
                // Join is a special case where we don't create a new component, but rather connect two existing wires.
                case InstanceType::Join:
                {
                    auto* join = static_cast<JoinInstance*>(component.get());
                    auto* emitter = findWire(join->emitter);
                    auto* receiver = findWire(join->receiver);
                    emitter->addTarget(receiver);
                }
                break;

                case InstanceType::Constant:
                {
                    auto* constant = static_cast<ConstantInstance*>(component.get());
                    auto* out = findWire(constant->out);
                    components.insert({ name, std::make_unique<Constant>(out, constant->value) });
                }
                break;

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
                break;

                case InstanceType::Shifter:
                {
                    auto* shifter = static_cast<ShifterInstance*>(component.get());
                    auto* in = findWire(shifter->in);
                    auto* shamt = findWire(shifter->shamt);
                    auto* out = findWire(shifter->out);
                    components.insert({ name, std::make_unique<Shifter>(in, shamt, out, shifter->op) });
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

                case InstanceType::Concatenator:
                {
                    auto* concatenator = static_cast<ConcatenatorInstance*>(component.get());
                    auto* low = findWire(concatenator->low);
                    auto* high = findWire(concatenator->high);
                    auto* out = findWire(concatenator->out);
                    components.insert({ name, std::make_unique<Concatenator>(low, high, out) });
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

                case InstanceType::Process:
                {
                    auto* process = static_cast<ProcessInstance*>(component.get());

                    // Map the process's ports to the parent subgraph's wires
                    PortInitializer inPorts, outPorts;

                    for (const auto& portName : process->inPorts)
                    {
                        Wire* parentWire = findWire(portName);
                        if (!parentWire)
                            throw std::runtime_error("Process construction failed: Input port '" + portName + "' not found in parent subgraph.");
                        inPorts.emplace_back(portName, parentWire);
                    }

                    for (const auto& portName : process->outPorts)
                    {
                        Wire* parentWire = findWire(portName);
                        if (!parentWire)
                            throw std::runtime_error("Process construction failed: Output port '" + portName + "' not found in parent subgraph.");
                        outPorts.emplace_back(portName, parentWire);
                    }

                    if (process->sensList.empty())
                    {
                        components.insert({ name, std::make_unique<SequentialProcessBox>(inPorts, outPorts, std::move(process->instructions)) });
                    }
                    else
                    {
                        std::vector<Wire*> sensList;
                        for (const auto& sensName : process->sensList)
                        {
                            Wire* sensWire = findWire(sensName);
                            if (!sensWire)
                                throw std::runtime_error("Process construction failed: Sensitivity list wire '" + sensName + "' not found in parent subgraph.");
                            sensList.push_back(sensWire);
                        }
                        components.insert({ name, std::make_unique<CombinationalProcessBox>(inPorts, outPorts, std::move(process->instructions), sensList) });
                    }
                }
                break;

                // Subgraphs are nested components, so we need to recursively create them.
                case InstanceType::Subgraph:
                {
                    if (std::find(visitedSubgraphs.begin(), visitedSubgraphs.end(), name) != visitedSubgraphs.end())
                    {
                        throw std::runtime_error("Subgraph construction failed: Recursive subgraph detected at: " + name);
                    }

                    auto* subgraph = static_cast<SubgraphInstance*>(component.get());
                    auto* bp = subgraph->bp;
                    auto& portMap = subgraph->portMap;

                    // Map the subgraph's ports to the parent subgraph's wires
                    PortInitializer inPorts, outPorts;

                    for (const auto& portName : subgraph->bp->inPorts)
                    {
                        auto it = portMap.find(portName);
                        if (it == portMap.end())
                            continue; // No signal is connected to this port, skip it.

                        Wire* parentWire = findWire(it->second);
                        inPorts.emplace_back(portName, parentWire);
                    }

                    for (const auto& portName : subgraph->bp->outPorts)
                    {
                        auto it = portMap.find(portName);
                        if (it == portMap.end())
                            continue; // No signal is connected to this port, skip it.

                        Wire* parentWire = findWire(it->second);
                        outPorts.emplace_back(portName, parentWire);
                    }

                    visitedSubgraphs.push_back(name);
                    components.insert({ name, std::make_unique<Subgraph>(*bp, inPorts, outPorts, visitedSubgraphs) });
                    visitedSubgraphs.pop_back();
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

    void Subgraph::update()
    {
        for (auto& [name, component] : components)
            component->update();
    }

    SubgraphSnapshot Subgraph::takeSnapshot() const
    {
        SubgraphSnapshot snapshot;

        // Capture the state of input ports
        for (const auto& [name, signal] : m_inSignals)
        {
            if (name[0] == '$') continue;
            snapshot.inputs[name] = { signal->width(), signal->peek() };
        }
        
        // Capture the state of output ports
        for (const auto& [name, signal] : m_outSignals)
        {
            if (name[0] == '$') continue;
            snapshot.outputs[name] = { signal->width(), signal->peek() };
        }

        // Capture the state of internal wires
        for (const auto& [name, wire] : wires)
        {
            if (name[0] == '$') continue;
            snapshot.wires[name] = { wire->width(), wire->peek() };
        }

        // Capture the state of internal components (subgraphs)
        for (const auto& [name, component] : components)
        {
            if (auto subgraph = dynamic_cast<Subgraph*>(component.get()))
            {
                snapshot.subgraphs[name] = subgraph->takeSnapshot();
            }
        }

        return snapshot;
    }
}