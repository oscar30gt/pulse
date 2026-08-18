#ifndef PULSE_BLUEPRINT_H
#define PULSE_BLUEPRINT_H

#include <string>
#include <unordered_map>
#include <memory>

#include "signalInterface.h"
#include "gates.h"
#include "shifter.h"
#include "comparator.h"

namespace Pulse::Parser
{
    enum class InstanceType : uint8_t
    {
        None,
        BinaryGate,
        Shifter,
        Comparator,
        Splitter,
        NotGate,
        Merger,
        Adder,
        Subtractor,
        Multiplicator,
        ControlledBuffer,
        Subgraph
    };

    // --------------------------------------------------------------------------------------------

    /// Definition of a signal within a component
    struct WireInstance
    {
        bitWidth_t width;
    };

    // --------------------------------------------------------------------------------------------

    struct Blueprint;

    /// Struct for component instances.
    /// A component instance defines, rather than its architecture, 
    /// the properties of a certain component instance.
    struct ComponentInstance
    {
        const InstanceType type = InstanceType::None;
        virtual ~ComponentInstance() = default;

    protected:
        // Protected constructor prevents manual instantiation
        ComponentInstance(InstanceType t) : type(t) { }
    };

    /// Binary gate instance: NOT, AND, OR, XOR, NAND, NOR, XNOR
    struct BinaryGateInstance : ComponentInstance
    {
        std::string in0, in1, out;
        Pulse::Engine::BinaryOp op;

        BinaryGateInstance(std::string in0, std::string in1, std::string out, Pulse::Engine::BinaryOp op)
            : ComponentInstance(InstanceType::BinaryGate), in0(in0), in1(in1), out(out), op(op)
        { }
    };

    /// Not gate instance
    struct NotGateInstance : ComponentInstance
    {
        std::string in, out;

        NotGateInstance(std::string in, std::string out)
            : ComponentInstance(InstanceType::NotGate), in(in), out(out)
        { }
    };

    /// Shifter instance: LSL, LSR, ASR, ROL, ROR
    struct ShifterInstance : ComponentInstance
    {
        std::string in, shamt, out;
        Pulse::Engine::ShiftOp op;

        ShifterInstance(std::string in, std::string shamt, std::string out, Pulse::Engine::ShiftOp op)
            : ComponentInstance(InstanceType::Shifter), in(in), shamt(shamt), out(out), op(op)
        { }
    };

    /// Comparator instance: EQ, NE, LT, LE, GT, GE (signed or unsigned)
    struct ComparatorInstance : ComponentInstance
    {
        std::string in0, in1, out;
        Pulse::Engine::CompareOp op;
        Pulse::Engine::CompareMode mode;

        ComparatorInstance(std::string in0, std::string in1, std::string out, Pulse::Engine::CompareOp op, Pulse::Engine::CompareMode mode)
            : ComponentInstance(InstanceType::Comparator), in0(in0), in1(in1), out(out), op(op), mode(mode)
        { }
    };

    /// Splitter instance: extracts (high, low) range
    struct SplitterInstance : ComponentInstance
    {
        std::string in, out;
        bitWidth_t high;
        bitWidth_t low;

        SplitterInstance(std::string in, std::string out, bitWidth_t high, bitWidth_t low)
            : ComponentInstance(InstanceType::Splitter), in(in), out(out), high(high), low(low)
        { }
    };

    /// Merger (concatenator) instance
    struct MergerInstance : ComponentInstance
    {
        std::string low, high, out;

        MergerInstance(std::string low, std::string high, std::string out)
            : ComponentInstance(InstanceType::Merger), low(low), high(high), out(out)
        { }
    };

    /// Adder instance
    struct AdderInstance : ComponentInstance
    {
        std::string in0, in1, out;

        AdderInstance(std::string in0, std::string in1, std::string out)
            : ComponentInstance(InstanceType::Adder), in0(in0), in1(in1), out(out)
        { }
    };

    // Subtractor instance
    struct SubtractorInstance : ComponentInstance
    {
        std::string in0, in1, out;

        SubtractorInstance(std::string in0, std::string in1, std::string out)
            : ComponentInstance(InstanceType::Subtractor), in0(in0), in1(in1), out(out)
        { }
    };

    // Multiplicator instance
    struct MultiplicatorInstance : ComponentInstance
    {
        std::string in0, in1, out;

        MultiplicatorInstance(std::string in0, std::string in1, std::string out)
            : ComponentInstance(InstanceType::Multiplicator), in0(in0), in1(in1), out(out)
        { }
    };

    /// Controlled buffer (tri-state buffer) instance
    struct ControlledBufferInstance : ComponentInstance
    {
        std::string in, out, enable;

        ControlledBufferInstance(std::string in, std::string out, std::string enable)
            : ComponentInstance(InstanceType::ControlledBuffer), in(in), out(out), enable(enable)
        { }
    };

    /// Instance of a nested subgraph
    struct SubgraphInstance : ComponentInstance
    {
        std::unordered_map<std::string, std::string> portMap;
        Blueprint* bp;

        SubgraphInstance(Blueprint* bp, std::unordered_map<std::string, std::string> portMap)
            : ComponentInstance(InstanceType::Subgraph), bp(bp), portMap(std::move(portMap))
        { }
    };

    // --------------------------------------------------------------------------------------------

    /// Defines the architecture of a component so it can
    /// be dynamically instantiated via the subgraph component
    struct Blueprint
    {
        std::vector<std::string> inPorts;
        std::vector<std::string> outPorts;
        std::unordered_map<std::string, WireInstance> wires;
        std::unordered_map<std::string, std::unique_ptr<ComponentInstance>> components;

        /// Adds an input/output port to the blueprint.
        void addPort(std::string name, bool isInput);

        /// Adds an internal wire/signal instance to the blueprint.
        void addSignal(std::string name, bitWidth_t width);

        /// Instantiates and registers a component of type T into the blueprint.
        /// Blueprint will take ownership of the added instance.
        void addComponent(std::string name, std::unique_ptr<ComponentInstance> instance);
    };

    // --------------------------------------------------------------------------------------------

    inline void Blueprint::addPort(std::string name, bool isInput)
    {
        if (isInput) inPorts.push_back(std::move(name));
        else outPorts.push_back(std::move(name));
    }

    inline void Blueprint::addSignal(std::string name, bitWidth_t width)
    {
        wires[std::move(name)] = WireInstance{ width };
    }

    inline void Blueprint::addComponent(std::string name, std::unique_ptr<ComponentInstance> instance)
    {
        if (instance)
        {
            components[std::move(name)] = std::move(instance);
        }
    }
}

#endif // PULSE_BLUEPRINT_H