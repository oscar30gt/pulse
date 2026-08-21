#include "blueprint.h"
#include <iostream>
#include <iomanip>
#include <string>

namespace Pulse::Parser
{
    class BlueprintPrinter
    {
    public:
        // Prints a formatted summary of a Blueprint circuit to the specified output stream
        static void print(const Blueprint& bp, std::ostream& os = std::cout)
        {
            os << "========================================\n";
            os << "           BLUEPRINT CIRCUIT            \n";
            os << "========================================\n\n";

            // 1. Ports
            os << "[ Ports ]\n";
            os << "  Input Ports (" << bp.inPorts.size() << "): ";
            for (size_t i = 0; i < bp.inPorts.size(); ++i)
            {
                os << bp.inPorts[i] << (i + 1 < bp.inPorts.size() ? ", " : "");
            }
            os << "\n";

            os << "  Output Ports (" << bp.outPorts.size() << "): ";
            for (size_t i = 0; i < bp.outPorts.size(); ++i)
            {
                os << bp.outPorts[i] << (i + 1 < bp.outPorts.size() ? ", " : "");
            }
            os << "\n\n";

            // 2. Wires
            os << "[ Wires / Signals (" << bp.wires.size() << ") ]\n";
            for (const auto& [wireName, wireInst] : bp.wires)
            {
                os << "  - " << std::left << std::setw(20) << wireName 
                   << " (width: " << std::to_string(wireInst.width) << ")\n";
            }
            os << "\n";

            // 3. Components
            os << "[ Component Instances (" << bp.components.size() << ") ]\n";
            for (const auto& [compName, compPtr] : bp.components)
            {
                printComponent(compName, compPtr.get(), os);
            }

            os << "========================================\n";
        }

    private:
        static std::string binaryOpToString(Pulse::Engine::BinaryOp op)
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

        static std::string shiftOpToString(Pulse::Engine::ShiftOp op)
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

        static std::string compareOpToString(Pulse::Engine::CompareOp op)
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

        static std::string compareModeToString(Pulse::Engine::CompareMode mode)
        {
            switch (mode)
            {
                case Pulse::Engine::CompareMode::Unsigned: return "Unsigned";
                case Pulse::Engine::CompareMode::Signed:   return "Signed";
                default: return "UNKNOWN";
            }
        }

        static void printComponent(const std::string& name, const ComponentInstance* comp, std::ostream& os)
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
    };
} // namespace Pulse::Parser