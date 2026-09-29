#include "subgraph.h"

#include <algorithm>
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
    Subgraph::RootPorts Subgraph::makeRootPorts(const Blueprint& bp)
    {
        RootPorts ports;

        const auto create = [&](const std::string& name, PortInitializer& list)
        {
            auto info = bp.portWires.find(name);
            const WireInstance wire = info != bp.portWires.end() ? info->second : WireInstance{};
            ports.owned.push_back(std::make_unique<Wire>(wire.width, wire.defaultValue));
            list.emplace_back(name, ports.owned.back().get());
        };

        for (const auto& name : bp.inPorts) create(name, ports.inPorts);
        for (const auto& name : bp.outPorts) create(name, ports.outPorts);
        return ports;
    }

    Subgraph::Subgraph(const Blueprint& bp)
        : Subgraph(bp, makeRootPorts(bp))
    { }

    Subgraph::Subgraph(const Blueprint& bp, RootPorts ports)
        : Component(ports.inPorts, ports.outPorts),
        m_ownedPorts(std::move(ports.owned)),
        m_blueprint(&bp)
    {
        std::vector<const Blueprint*> visitedBlueprints{ &bp };
        build(bp, visitedBlueprints);
    }

    Subgraph::Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts)
        : Component(inPorts, outPorts),
        m_blueprint(&bp)
    {
        std::vector<const Blueprint*> visitedBlueprints{ &bp };
        build(bp, visitedBlueprints);
    }

    Subgraph::Subgraph(const Blueprint& bp, const PortInitializer& inPorts, const PortInitializer& outPorts, std::vector<const Blueprint*>& visitedBlueprints)
        : Component(inPorts, outPorts),
        m_blueprint(&bp)
    {
        build(bp, visitedBlueprints);
    }

    void Subgraph::build(const Blueprint& bp, std::vector<const Blueprint*>& visitedBlueprints)
    {
        // Wires must be created before components, as components need
        // to connect to the wires during their construction.
        for (auto& [name, wire] : bp.wires)
        {
            wires.insert({ name, std::make_unique<Wire>(wire.width, wire.defaultValue) });
        }

        // Now, create the components based on the blueprint.
        // Each component will connect to the appropriate wires,
        // which can be either internal wires or the subgraph's ports.
        for (auto& [name, component] : bp.components)
        {
            const std::string user = "component '" + name + "'";

            // Instantiate the component based on its type. Subgraph will take ownership of the created component.
            switch (component->type)
            {
                // Join is a special case where we don't create a new component, but rather connect two existing wires.
                case InstanceType::Join:
                {
                    auto* join = static_cast<const JoinInstance*>(component.get());
                    auto* emitter = requireWire(join->emitter, user);
                    auto* receiver = requireWire(join->receiver, user);
                    emitter->addTarget(receiver);
                }
                break;

                case InstanceType::Constant:
                {
                    auto* constant = static_cast<const ConstantInstance*>(component.get());
                    auto* out = requireWire(constant->out, user);
                    components.insert({ name, std::make_unique<Constant>(out, constant->value) });
                }
                break;

                case InstanceType::BinaryGate:
                {
                    auto* gate = static_cast<const BinaryGateInstance*>(component.get());
                    auto* in0 = requireWire(gate->in0, user);
                    auto* in1 = requireWire(gate->in1, user);
                    auto* out = requireWire(gate->out, user);
                    components.insert({ name, std::make_unique<BinaryGate>(in0, in1, out, gate->op) });
                }
                break;

                case InstanceType::NotGate:
                {
                    auto* notGate = static_cast<const NotGateInstance*>(component.get());
                    auto* in = requireWire(notGate->in, user);
                    auto* out = requireWire(notGate->out, user);
                    components.insert({ name, std::make_unique<NOTGate>(in, out) });
                }
                break;

                case InstanceType::Shifter:
                {
                    auto* shifter = static_cast<const ShifterInstance*>(component.get());
                    auto* in = requireWire(shifter->in, user);
                    auto* shamt = requireWire(shifter->shamt, user);
                    auto* out = requireWire(shifter->out, user);
                    components.insert({ name, std::make_unique<Shifter>(in, shamt, out, shifter->op) });
                }
                break;

                case InstanceType::Comparator:
                {
                    auto* comparator = static_cast<const ComparatorInstance*>(component.get());
                    auto* in0 = requireWire(comparator->in0, user);
                    auto* in1 = requireWire(comparator->in1, user);
                    auto* out = requireWire(comparator->out, user);
                    components.insert({ name, std::make_unique<Comparator>(in0, in1, out, comparator->op, comparator->mode) });
                }
                break;

                case InstanceType::Splitter:
                {
                    auto* splitter = static_cast<const SplitterInstance*>(component.get());
                    auto* in = requireWire(splitter->in, user);
                    auto* out = requireWire(splitter->out, user);
                    components.insert({ name, std::make_unique<Splitter>(in, out, std::pair(splitter->high, splitter->low)) });
                }
                break;

                case InstanceType::Concatenator:
                {
                    auto* concatenator = static_cast<const ConcatenatorInstance*>(component.get());
                    auto* low = requireWire(concatenator->low, user);
                    auto* high = requireWire(concatenator->high, user);
                    auto* out = requireWire(concatenator->out, user);
                    components.insert({ name, std::make_unique<Concatenator>(low, high, out) });
                }
                break;

                case InstanceType::Adder:
                {
                    auto* adder = static_cast<const AdderInstance*>(component.get());
                    auto* in0 = requireWire(adder->in0, user);
                    auto* in1 = requireWire(adder->in1, user);
                    auto* out = requireWire(adder->out, user);
                    components.insert({ name, std::make_unique<Adder>(in0, in1, out) });
                }
                break;

                case InstanceType::Subtractor:
                {
                    auto* subtractor = static_cast<const SubtractorInstance*>(component.get());
                    auto* in0 = requireWire(subtractor->in0, user);
                    auto* in1 = requireWire(subtractor->in1, user);
                    auto* out = requireWire(subtractor->out, user);
                    components.insert({ name, std::make_unique<Subtractor>(in0, in1, out) });
                }
                break;

                case InstanceType::Multiplicator:
                {
                    auto* multiplicator = static_cast<const MultiplicatorInstance*>(component.get());
                    auto* in0 = requireWire(multiplicator->in0, user);
                    auto* in1 = requireWire(multiplicator->in1, user);
                    auto* out = requireWire(multiplicator->out, user);
                    components.insert({ name, std::make_unique<Multiplicator>(in0, in1, out) });
                }
                break;

                case InstanceType::ControlledBuffer:
                {
                    auto* buffer = static_cast<const ControlledBufferInstance*>(component.get());
                    auto* in = requireWire(buffer->in, user);
                    auto* enable = requireWire(buffer->enable, user);
                    auto* out = requireWire(buffer->out, user);
                    components.insert({ name, std::make_unique<ControlledBuffer>(in, enable, out) });
                }
                break;

                case InstanceType::EventProbe:
                {
                    auto* probe = static_cast<const EventProbeInstance*>(component.get());
                    auto* in = requireWire(probe->in, user);
                    auto* out = requireWire(probe->out, user);
                    m_probes.push_back(std::make_unique<EventProbe>(in, out));
                }
                break;

                case InstanceType::Process:
                {
                    auto* process = static_cast<const ProcessInstance*>(component.get());

                    // Map the process's ports to the parent subgraph's wires
                    PortInitializer inPorts, outPorts;

                    for (const auto& portName : process->inPorts)
                        inPorts.emplace_back(portName, requireWire(portName, "input port of process '" + name + "'"));

                    for (const auto& portName : process->outPorts)
                        outPorts.emplace_back(portName, requireWire(portName, "output port of process '" + name + "'"));

                    if (process->triggers.empty() && !process->combinational)
                    {
                        components.insert({ name, std::make_unique<SequentialProcessBox>(inPorts, outPorts, process->instructions) });
                    }
                    else
                    {
                        std::vector<Wire*> triggers;
                        for (const auto& trigger : process->triggers)
                            triggers.push_back(requireWire(trigger, "triggers of process '" + name + "'"));
                        components.insert({ name, std::make_unique<CombinationalProcessBox>(inPorts, outPorts, process->instructions, triggers) });
                    }
                }
                break;

                // Subgraphs are nested components, so we need to recursively create them.
                case InstanceType::Subgraph:
                {
                    auto* subgraph = static_cast<const SubgraphInstance*>(component.get());
                    const Blueprint* childBlueprint = subgraph->bp;
                    if (!childBlueprint)
                        throw std::runtime_error("Subgraph construction failed: subgraph '" + name + "' has no blueprint.");

                    if (std::find(visitedBlueprints.begin(), visitedBlueprints.end(), childBlueprint) != visitedBlueprints.end())
                        throw std::runtime_error("Subgraph construction failed: Recursive subgraph detected at: " + name);

                    auto& portMap = subgraph->portMap;

                    // Map the subgraph's ports to the parent subgraph's wires
                    PortInitializer inPorts, outPorts;

                    for (const auto& portName : childBlueprint->inPorts)
                    {
                        auto it = portMap.find(portName);
                        if (it == portMap.end())
                            continue; // No signal is connected to this port, skip it.

                        inPorts.emplace_back(portName, requireWire(it->second, "port '" + portName + "' of subgraph '" + name + "'"));
                    }

                    for (const auto& portName : childBlueprint->outPorts)
                    {
                        auto it = portMap.find(portName);
                        if (it == portMap.end())
                            continue; // No signal is connected to this port, skip it.

                        outPorts.emplace_back(portName, requireWire(it->second, "port '" + portName + "' of subgraph '" + name + "'"));
                    }

                    visitedBlueprints.push_back(childBlueprint);
                    auto child = std::make_unique<Subgraph>(*childBlueprint, inPorts, outPorts, visitedBlueprints);
                    visitedBlueprints.pop_back();

                    m_children.emplace_back(name, child.get());
                    components.insert({ name, std::move(child) });
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

    Wire* Subgraph::requireWire(const std::string& name, const std::string& user)
    {
        Wire* wire = findWire(name);
        if (!wire)
            throw std::runtime_error("Subgraph construction failed: wire '" + name + "' used by " + user + " does not exist.");
        return wire;
    }

    void Subgraph::latchEvents()
    {
        for (auto& probe : m_probes)
            probe->latch();

        for (auto& [name, child] : m_children)
            child->latchEvents();
    }

    void Subgraph::publishEvents()
    {
        for (auto& probe : m_probes)
            probe->publish();

        for (auto& [name, child] : m_children)
            child->publishEvents();
    }

    void Subgraph::tick()
    {
        latchEvents();
        publishEvents();
        update();
        commit();
    }

    void Subgraph::update()
    {
        for (auto& [name, component] : components)
            component->update();
    }

    void Subgraph::commit()
    {
        for (auto& [name, component] : components)
            component->commit();
    }

    SubgraphSnapshot Subgraph::takeSnapshot() const
    {
        SubgraphSnapshot snapshot;
        snapshot.symbols = &m_blueprint->symbols;

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
        for (const auto& [name, child] : m_children)
            snapshot.subgraphs[name] = child->takeSnapshot();

        return snapshot;
    }
}
