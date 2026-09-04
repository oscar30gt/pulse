#ifndef PULSE_BLUEPRINT_H
#define PULSE_BLUEPRINT_H

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <iostream>
#include <iomanip>

#include "signalInterface.h"
#include "gates.h"
#include "shifter.h"
#include "comparator.h"
#include "processBox.h"

namespace Pulse::Engine
{
    struct ComponentInstance;

    inline std::string binaryOpToString(BinaryOp op);
    inline std::string shiftOpToString(ShiftOp op);
    inline std::string compareOpToString(CompareOp op);
    inline std::string compareModeToString(CompareMode mode);
    inline void printComponent(const std::string& name, const ComponentInstance* comp, std::ostream& os = std::cout);

    // --------------------------------------------------------------------------------------------

    enum class InstanceType : uint8_t
    {
        None,
        BinaryGate,
        Shifter,
        Comparator,
        Splitter,
        NotGate,
        Concatenator,
        Adder,
        Subtractor,
        Multiplicator,
        ControlledBuffer,
        Constant,
        Join,
        Process,
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

    /// Virual component that joins 2 signals
    struct JoinInstance : ComponentInstance
    {
        std::string emitter;
        std::string receiver;

        JoinInstance(std::string emitter, std::string receiver)
            : ComponentInstance(InstanceType::Join), emitter(emitter), receiver(receiver)
        { }
    };

    struct ConstantInstance : ComponentInstance
    {
        std::string out;
        LogicVector value;

        ConstantInstance(std::string out, LogicVector value)
            : ComponentInstance(InstanceType::Constant), out(out), value(value)
        { }
    };

    /// Binary gate instance: NOT, AND, OR, XOR, NAND, NOR, XNOR
    struct BinaryGateInstance : ComponentInstance
    {
        std::string in0, in1, out;
        BinaryOp op;

        BinaryGateInstance(std::string in0, std::string in1, std::string out, BinaryOp op)
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
        ShiftOp op;

        ShifterInstance(std::string in, std::string shamt, std::string out, ShiftOp op)
            : ComponentInstance(InstanceType::Shifter), in(in), shamt(shamt), out(out), op(op)
        { }
    };

    /// Comparator instance: EQ, NE, LT, LE, GT, GE (signed or unsigned)
    struct ComparatorInstance : ComponentInstance
    {
        std::string in0, in1, out;
        CompareOp op;
        CompareMode mode;

        ComparatorInstance(std::string in0, std::string in1, std::string out, CompareOp op, CompareMode mode)
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

    /// Concatenator instance (high&low)
    struct ConcatenatorInstance : ComponentInstance
    {
        std::string low, high, out;

        ConcatenatorInstance(std::string low, std::string high, std::string out)
            : ComponentInstance(InstanceType::Concatenator), low(low), high(high), out(out)
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

    /// VHDL process instance. A process is an abstraction of a sequential block of code that can be executed in a simulation.
    /// @note No port mapping is required for a process. It must be instantiated inside a subgraph that contains signals
    /// with the same names as the process's input and output ports.
    /// This is because a process is just an abstraction that is single-instantiated in a subgraph. Multiple gates can be instantiated
    /// inside a subgraph, but a process is a single block of code that just abstracts the logic of a sequential block of code.
    /// This is a design choice and could be changed in the future.
    struct ProcessInstance : ComponentInstance
    {
        /// Sensitivity list of the process (signals that trigger the process)
        /// Empty for processes whose execution is managed via "wait" statements.
        std::vector<std::string> sensList;
        
        std::vector<std::string> inPorts;
        std::vector<std::string> outPorts;
        std::vector<std::unique_ptr<ProcessInstruction>> instructions;

        ProcessInstance(std::vector<std::string> inPorts, std::vector<std::string> outPorts, std::vector<std::unique_ptr<ProcessInstruction>> instructions, std::vector<std::string> sensList = {})
            : ComponentInstance(InstanceType::Process), inPorts(std::move(inPorts)), outPorts(std::move(outPorts)), instructions(std::move(instructions)), sensList(std::move(sensList))
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

        /// Prints a formatted summary of the blueprint to the specified output stream.
        /// @param os The output stream to print to. Defaults to std::cout.
        void print(std::ostream& os = std::cout) const;
    };

} // namespace Pulse::Engine

#include "blueprint_impl.h"

#endif // PULSE_BLUEPRINT_H