#ifndef PULSE_BLUEPRINT_IMPL_H
#include "blueprint.h" // Not necessary, but included to enable IDEs to recognize the Blueprint class and its members.
#endif // PULSE_BLUEPRINT_IMPL_H

namespace Pulse::Engine
{
    inline void Blueprint::addPort(std::string name, bool isInput, WireInstance wire)
    {
        portWires[name] = wire;
        if (isInput) inPorts.push_back(std::move(name));
        else outPorts.push_back(std::move(name));
    }

    inline void Blueprint::addSignal(std::string name, bitWidth_t width, LogicVector defaultValue)
    {
        wires[std::move(name)] = WireInstance{ width, defaultValue };
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
                << " (width: " << std::to_string(wireInst.width) << ", default: " << wireInst.defaultValue.str(wireInst.width) << ")\n";
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

    inline std::string binaryOpToString(BinaryOp op)
    {
        switch (op)
        {
            case BinaryOp::AND:  return "AND";
            case BinaryOp::OR:   return "OR";
            case BinaryOp::XOR:  return "XOR";
            case BinaryOp::NAND: return "NAND";
            case BinaryOp::NOR:  return "NOR";
            case BinaryOp::XNOR: return "XNOR";
            default: return "UNKNOWN";
        }
    }

    inline std::string shiftOpToString(ShiftOp op)
    {
        switch (op)
        {
            case ShiftOp::LogicalLeft: return "LSL";
            case ShiftOp::LogicalRight: return "LSR";
            case ShiftOp::ArithmeticRight: return "ASR";
            case ShiftOp::RotateLeft: return "ROL";
            case ShiftOp::RotateRight: return "ROR";
            default: return "UNKNOWN";
        }
    }

    inline std::string compareOpToString(CompareOp op)
    {
        switch (op)
        {
            case CompareOp::Equals:        return "EQ";
            case CompareOp::NotEquals:     return "NE";
            case CompareOp::LessThan:      return "LT";
            case CompareOp::LessThanEqual:    return "LE";
            case CompareOp::GreaterThan:  return "GT";
            case CompareOp::GreaterThanEqual: return "GE";
            default: return "UNKNOWN";
        }
    }

    inline std::string compareModeToString(CompareMode mode)
    {
        switch (mode)
        {
            case CompareMode::Unsigned: return "Unsigned";
            case CompareMode::Signed:   return "Signed";
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
            case InstanceType::Process:
            {
                auto proc = static_cast<const ProcessInstance*>(comp);
                os << "[Process]";
                os << "\n      sensitivity (" << proc->sensList.size() << "): ";
                for (size_t i = 0; i < proc->sensList.size(); ++i)
                {
                    os << proc->sensList[i] << (i + 1 < proc->sensList.size() ? ", " : "");
                }
                os << "\n      inPorts (" << proc->inPorts.size() << "): ";
                for (size_t i = 0; i < proc->inPorts.size(); ++i)
                {
                    os << proc->inPorts[i] << (i + 1 < proc->inPorts.size() ? ", " : "");
                }
                os << "\n      outPorts (" << proc->outPorts.size() << "): ";
                for (size_t i = 0; i < proc->outPorts.size(); ++i)
                {
                    os << proc->outPorts[i] << (i + 1 < proc->outPorts.size() ? ", " : "");
                }
                os << "\n      Instructions (" << proc->instructions.size() << "):\n";
                for (size_t i = 0; i < proc->instructions.size(); ++i)
                {
                    const ProcessInstruction& inst = proc->instructions[i];
                    os << "        [" << i << "] ";
                    switch (inst.kind)
                    {
                        case ProcessInstructionKind::Assignment:
                        {
                            const auto& assign = static_cast<const ProcessInstructionAssignment&>(inst);
                            os << (assign.deferred ? "ASSIGN (deferred): " : "ASSIGN: ") << assign.targetPort << " <= " << assign.sourcePort << "\n";
                            break;
                        }
                        case ProcessInstructionKind::Branch:
                        {
                            const auto& branch = static_cast<const ProcessInstructionBranch&>(inst);
                            os << "BRANCH: unless " << branch.conditionPort << " == 1, skip " << branch.branchLength << " instructions\n";
                            break;
                        }
                        case ProcessInstructionKind::BranchAlways:
                            os << "BRANCH_ALWAYS: skip " << static_cast<const ProcessInstructionBranchAlways&>(inst).branchLength << " instructions\n";
                            break;
                        case ProcessInstructionKind::Wait:
                            os << "WAIT: " << static_cast<const ProcessInstructionWait&>(inst).waitTime << " fs\n";
                            break;
                        case ProcessInstructionKind::WaitForever:
                            os << "WAIT_FOREVER\n";
                            break;
                        case ProcessInstructionKind::WaitOn:
                        {
                            const auto& wait = static_cast<const ProcessInstructionWaitOn&>(inst);
                            os << "WAIT_ON:";
                            for (const auto& port : wait.sensitivity)
                                os << " " << port;
                            if (!wait.conditionPort.empty())
                                os << " until " << wait.conditionPort;
                            if (wait.hasTimeout)
                                os << " for " << wait.timeout << " fs";
                            os << "\n";
                            break;
                        }
                    }
                }
                break;
            }

            case InstanceType::EventProbe:
            {
                auto probe = static_cast<const EventProbeInstance*>(comp);
                os << "[EventProbe]\n"
                    << "      in: " << probe->in << " -> out: " << probe->out << "\n";
                break;
            }

            case InstanceType::Join:
            {
                auto join = static_cast<const JoinInstance*>(comp);
                os << "[Join]\n"
                    << "      emitter: " << join->emitter << " -> receiver: " << join->receiver << "\n";
                break;
            }

            default:
                os << "[Unknown InstanceType]\n";
                break;
        }
    }
}