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

namespace Pulse::Parser
{
    struct ComponentInstance;

    inline std::string binaryOpToString(Pulse::Engine::BinaryOp op);
    inline std::string shiftOpToString(Pulse::Engine::ShiftOp op);
    inline std::string compareOpToString(Pulse::Engine::CompareOp op);
    inline std::string compareModeToString(Pulse::Engine::CompareMode mode);
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

    inline void Blueprint::print(std::ostream& os) const
    {
        os << "===============================================\n";
        os << "                   BLUEPRINT                   \n";
        os << "===============================================\n\n";

        // 1. Ports
        os << "[ Ports ]\n";
        os << "  Input Ports (" << this->inPorts.size() << "): ";
        for (size_t i = 0; i < this->inPorts.size(); ++i)
        {
            os << this->inPorts[i] << (i + 1 < this->inPorts.size() ? ", " : "");
        }
        os << "\n";

        os << "  Output Ports (" << this->outPorts.size() << "): ";
        for (size_t i = 0; i < this->outPorts.size(); ++i)
        {
            os << this->outPorts[i] << (i + 1 < this->outPorts.size() ? ", " : "");
        }
        os << "\n\n";

        // 2. Wires
        os << "[ Wires / Signals (" << this->wires.size() << ") ]\n";
        for (const auto& [wireName, wireInst] : this->wires)
        {
            os << "  - " << std::left << std::setw(20) << wireName
                << " (width: " << std::to_string(wireInst.width) << ")\n";
        }
        os << "\n";

        // 3. Components
        os << "[ Component Instances (" << this->components.size() << ") ]\n";
        for (const auto& [compName, compPtr] : this->components)
        {
            printComponent(compName, compPtr.get(), os);
        }

        os << "========================================\n";
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
//// PRINTER //////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

    inline std::string binaryOpToString(Pulse::Engine::BinaryOp op)
    {
        switch (op)
        {
            case Pulse::Engine::BinaryOp::AND:  return "AND";
            case Pulse::Engine::BinaryOp::OR:   return "OR";
            case Pulse::Engine::BinaryOp::XOR:  return "XOR";
            case Pulse::Engine::BinaryOp::NAND: return "NAND";
            case Pulse::Engine::BinaryOp::NOR:  return "NOR";
            case Pulse::Engine::BinaryOp::XNOR: return "XNOR";
            default: return "UNKNOWN";
        }
    }

    inline std::string shiftOpToString(Pulse::Engine::ShiftOp op)
    {
        switch (op)
        {
            case Pulse::Engine::ShiftOp::LogicalLeft: return "LSL";
            case Pulse::Engine::ShiftOp::LogicalRight: return "LSR";
            case Pulse::Engine::ShiftOp::ArithmeticRight: return "ASR";
            case Pulse::Engine::ShiftOp::RotateLeft: return "ROL";
            case Pulse::Engine::ShiftOp::RotateRight: return "ROR";
            default: return "UNKNOWN";
        }
    }

    inline std::string compareOpToString(Pulse::Engine::CompareOp op)
    {
        switch (op)
        {
            case Pulse::Engine::CompareOp::Equals:        return "EQ";
            case Pulse::Engine::CompareOp::NotEquals:     return "NE";
            case Pulse::Engine::CompareOp::LessThan:      return "LT";
            case Pulse::Engine::CompareOp::LessThanEqual:    return "LE";
            case Pulse::Engine::CompareOp::GreaterThan:  return "GT";
            case Pulse::Engine::CompareOp::GreaterThanEqual: return "GE";
            default: return "UNKNOWN";
        }
    }

    inline std::string compareModeToString(Pulse::Engine::CompareMode mode)
    {
        switch (mode)
        {
            case Pulse::Engine::CompareMode::Unsigned: return "Unsigned";
            case Pulse::Engine::CompareMode::Signed:   return "Signed";
            default: return "UNKNOWN";
        }
    }

    inline void printComponent(const std::string& name, const ComponentInstance* comp, std::ostream& os)
    {
        if (!comp) return;

        os << "  + " << name << " ";

        switch (comp->type)
        {
            case InstanceType::BinaryGate:
            {
                auto gate = static_cast<const BinaryGateInstance*>(comp);
                os << "[BinaryGate: " << binaryOpToString(gate->op) << "]\n"
                    << "      in0: " << gate->in0 << ", in1: " << gate->in1
                    << " -> out: " << gate->out << "\n";
                break;
            }
            case InstanceType::NotGate:
            {
                auto gate = static_cast<const NotGateInstance*>(comp);
                os << "[NotGate]\n"
                    << "      in: " << gate->in << " -> out: " << gate->out << "\n";
                break;
            }
            case InstanceType::Shifter:
            {
                auto shifter = static_cast<const ShifterInstance*>(comp);
                os << "[Shifter: " << shiftOpToString(shifter->op) << "]\n"
                    << "      in: " << shifter->in << ", shamt: " << shifter->shamt
                    << " -> out: " << shifter->out << "\n";
                break;
            }
            case InstanceType::Comparator:
            {
                auto compInst = static_cast<const ComparatorInstance*>(comp);
                os << "[Comparator: " << compareOpToString(compInst->op)
                    << " (" << compareModeToString(compInst->mode) << ")]\n"
                    << "      in0: " << compInst->in0 << ", in1: " << compInst->in1
                    << " -> out: " << compInst->out << "\n";
                break;
            }
            case InstanceType::Splitter:
            {
                auto split = static_cast<const SplitterInstance*>(comp);
                os << "[Splitter]\n"
                    << "      in: " << split->in << " [" << split->high << " downto " << split->low << "]"
                    << " -> out: " << split->out << "\n";
                break;
            }
            case InstanceType::Concatenator:
            {
                auto merge = static_cast<const ConcatenatorInstance*>(comp);
                os << "[Concatenator]\n"
                    << "      high: " << merge->high << ", low: " << merge->low
                    << " -> out: " << merge->out << "\n";
                break;
            }
            case InstanceType::Adder:
            {
                auto adder = static_cast<const AdderInstance*>(comp);
                os << "[Adder]\n"
                    << "      in0: " << adder->in0 << ", in1: " << adder->in1
                    << " -> out: " << adder->out << "\n";
                break;
            }
            case InstanceType::Subtractor:
            {
                auto sub = static_cast<const SubtractorInstance*>(comp);
                os << "[Subtractor]\n"
                    << "      in0: " << sub->in0 << ", in1: " << sub->in1
                    << " -> out: " << sub->out << "\n";
                break;
            }
            case InstanceType::Multiplicator:
            {
                auto mult = static_cast<const MultiplicatorInstance*>(comp);
                os << "[Multiplicator]\n"
                    << "      in0: " << mult->in0 << ", in1: " << mult->in1
                    << " -> out: " << mult->out << "\n";
                break;
            }
            case InstanceType::ControlledBuffer:
            {
                auto buf = static_cast<const ControlledBufferInstance*>(comp);
                os << "[ControlledBuffer]\n"
                    << "      in: " << buf->in << ", enable: " << buf->enable
                    << " -> out: " << buf->out << "\n";
                break;
            }
            case InstanceType::Subgraph:
            {
                auto sub = static_cast<const SubgraphInstance*>(comp);
                os << "[Subgraph]\n"
                    << "      Port Mappings (" << sub->portMap.size() << "):\n";
                for (const auto& [subPort, parentWire] : sub->portMap)
                {
                    os << "        * " << subPort << " => " << parentWire << "\n";
                }
                break;
            }
            case InstanceType::Constant:
            {
                auto constant = static_cast<const ConstantInstance*>(comp);
                os << "[Constant]\n"
                    << "      value: " << constant->value.str() << " -> out: " << constant->out << "\n";
                break;
            }
            default:
                os << "[Unknown InstanceType]\n";
                break;
        }
    }

} // namespace Pulse::Parser

#endif // PULSE_BLUEPRINT_H