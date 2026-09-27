#ifndef PULSE_COMPILER_INSTRUCTION_BUILDER_H
#define PULSE_COMPILER_INSTRUCTION_BUILDER_H

// Builds the instruction list of a process box. Jumps go to labels, whose positions may be placed after the jump is
// emitted; finish() turns them into the forward skip lengths the process box executes.

#include "processBox.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Pulse::Parser
{
    class InstructionBuilder
    {
    public:
        using Label = size_t;

        /// A label to place later.
        Label newLabel()
        {
            m_labels.emplace_back();
            return m_labels.size() - 1;
        }

        /// Places a label before the next instruction.
        void place(Label label)
        {
            m_labels.at(label) = m_instructions.size();
        }

        /// `target <= source` (deferred, a signal) or `target := source` (immediate).
        void assign(const std::string& target, const std::string& source, bool deferred)
        {
            auto instruction = std::make_unique<Engine::ProcessInstructionAssignment>();
            instruction->targetPort = target;
            instruction->sourcePort = source;
            instruction->deferred = deferred;
            write(target);
            read(source);
            m_instructions.push_back(std::move(instruction));
        }

        /// Continues at `label` unless the 1-bit `condition` is '1'.
        void branchUnless(const std::string& condition, Label label)
        {
            auto instruction = std::make_unique<Engine::ProcessInstructionBranch>();
            instruction->conditionPort = condition;
            read(condition);
            m_fixups.emplace_back(m_instructions.size(), label);
            m_instructions.push_back(std::move(instruction));
        }

        /// Continues at `label`.
        void jump(Label label)
        {
            m_fixups.emplace_back(m_instructions.size(), label);
            m_instructions.push_back(std::make_unique<Engine::ProcessInstructionBranchAlways>());
        }

        void wait(simTime_t femtoseconds)
        {
            auto instruction = std::make_unique<Engine::ProcessInstructionWait>();
            instruction->waitTime = femtoseconds;
            m_instructions.push_back(std::move(instruction));
        }

        void waitForever()
        {
            m_instructions.push_back(std::make_unique<Engine::ProcessInstructionWaitForever>());
        }

        void waitOn(const std::vector<std::string>& sensitivity, const std::string& condition, std::optional<simTime_t> timeout)
        {
            auto instruction = std::make_unique<Engine::ProcessInstructionWaitOn>();
            instruction->sensitivity = sensitivity;
            instruction->conditionPort = condition;
            instruction->hasTimeout = timeout.has_value();
            instruction->timeout = timeout.value_or(0);
            for (const std::string& port : sensitivity) read(port);
            if (!condition.empty()) read(condition);
            m_instructions.push_back(std::move(instruction));
        }

        size_t size() const { return m_instructions.size(); }

        /// Wires the instructions only read, and wires they assign, each once and in order of first use.
        const std::vector<std::string>& reads() const { return m_reads; }
        const std::vector<std::string>& writes() const { return m_writes; }

        /// The instructions, with every jump resolved to the skip length the process box expects.
        std::vector<std::unique_ptr<Engine::ProcessInstruction>> finish()
        {
            for (const auto& [index, label] : m_fixups)
            {
                const std::optional<size_t>& position = m_labels.at(label);
                if (!position || *position <= index)
                    throw std::logic_error("InstructionBuilder: a jump must go forward to a placed label");

                const size_t length = *position - index - 1;
                auto& instruction = *m_instructions[index];
                if (instruction.kind == Engine::ProcessInstructionKind::Branch)
                    static_cast<Engine::ProcessInstructionBranch&>(instruction).branchLength = length;
                else
                    static_cast<Engine::ProcessInstructionBranchAlways&>(instruction).branchLength = length;
            }
            m_fixups.clear();
            return std::move(m_instructions);
        }

    private:
        std::vector<std::unique_ptr<Engine::ProcessInstruction>> m_instructions;
        std::vector<std::optional<size_t>> m_labels;
        std::vector<std::pair<size_t, Label>> m_fixups;
        std::vector<std::string> m_reads;
        std::vector<std::string> m_writes;

        static void addOnce(std::vector<std::string>& list, const std::string& wire)
        {
            for (const std::string& known : list)
                if (known == wire) return;
            list.push_back(wire);
        }

        void read(const std::string& wire) { addOnce(m_reads, wire); }
        void write(const std::string& wire) { addOnce(m_writes, wire); }
    };

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_INSTRUCTION_BUILDER_H
