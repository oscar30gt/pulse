#include "processBox.h"

#include <algorithm>
#include <stdexcept>

namespace Pulse::Engine
{
    // -------- Program ---------------------------------------------------------------------------

    ProcessProgram::ProcessProgram()
        : m_instructions(std::make_shared<const std::vector<std::unique_ptr<ProcessInstruction>>>())
    { }

    ProcessProgram::ProcessProgram(std::vector<std::unique_ptr<ProcessInstruction>> instructions)
        : m_instructions(std::make_shared<const std::vector<std::unique_ptr<ProcessInstruction>>>(std::move(instructions)))
    {
        for (const auto& instruction : *m_instructions)
            if (!instruction)
                throw std::invalid_argument("A process program cannot contain a null instruction.");
    }

    size_t ProcessProgram::size() const
    {
        return m_instructions->size();
    }

    bool ProcessProgram::empty() const
    {
        return m_instructions->empty();
    }

    const ProcessInstruction& ProcessProgram::operator[](size_t index) const
    {
        return *(*m_instructions)[index];
    }

    // -------- Process box -----------------------------------------------------------------------

    ProcessBox::ProcessBox(
        const PortInitializer& inPorts,
        const PortInitializer& outPorts,
        ProcessProgram instructions
    ) : Component(inPorts, outPorts),
        m_instructions(std::move(instructions))
    {
        const auto wireOf = [this](const std::string& port) -> Wire*
        {
            Wire* wire = nullptr;
            try {
                wire = getPort(port);
            }
            catch (const std::invalid_argument&) {
                throw std::runtime_error("ProcessBox construction failed: an instruction uses the unknown port '" + port + "'.");
            }
            if (!wire)
                throw std::runtime_error("ProcessBox construction failed: no wire is connected to port '" + port + "'.");
            return wire;
        };

        m_resolved.resize(m_instructions.size());
        for (size_t i = 0; i < m_instructions.size(); ++i)
        {
            const ProcessInstruction& instruction = m_instructions[i];
            ResolvedInstruction& resolved = m_resolved[i];

            switch (instruction.kind)
            {
                case ProcessInstructionKind::Assignment:
                {
                    const auto& assignment = static_cast<const ProcessInstructionAssignment&>(instruction);
                    resolved.first = wireOf(assignment.targetPort);
                    resolved.second = wireOf(assignment.sourcePort);
                    break;
                }
                case ProcessInstructionKind::Branch:
                    resolved.first = wireOf(static_cast<const ProcessInstructionBranch&>(instruction).conditionPort);
                    break;

                case ProcessInstructionKind::WaitOn:
                {
                    const auto& wait = static_cast<const ProcessInstructionWaitOn&>(instruction);
                    if (!wait.conditionPort.empty())
                        resolved.first = wireOf(wait.conditionPort);
                    for (const std::string& port : wait.triggers)
                        resolved.triggers.push_back(wireOf(port));
                    break;
                }
                default:
                    break;
            }
        }
    }

    ProcessBox::~ProcessBox() = default;

    bool ProcessBox::conditionMet(const Wire* condition)
    {
        const LogicVector value = condition->peek();
        return (value.mask & 1ULL) == 0 && (value.value & 1ULL) != 0;
    }

    void ProcessBox::assign(size_t index)
    {
        const auto& assignment = static_cast<const ProcessInstructionAssignment&>(m_instructions[index]);
        Wire* target = m_resolved[index].first;
        const LogicVector value = m_resolved[index].second->peek();

        if (!assignment.deferred)
        {
            target->drive(value);
            return;
        }

        // A later assignment to the same signal in the same run replaces the earlier one.
        auto pending = std::find_if(m_pending.begin(), m_pending.end(), [target](const auto& entry) { return entry.first == target; });
        if (pending != m_pending.end())
            pending->second = value;
        else
            m_pending.emplace_back(target, value);
    }

    void ProcessBox::commit()
    {
        // Moved out first: driving a wire may reach components that run again, never this process within a commit.
        std::vector<std::pair<Wire*, LogicVector>> pending = std::move(m_pending);
        m_pending.clear();

        for (const auto& [wire, value] : pending)
            wire->drive(value);
    }

    // --------------------------------------------------------------------------------------------

    SequentialProcessBox::SequentialProcessBox(
        const PortInitializer& inPorts,
        const PortInitializer& outPorts,
        ProcessProgram instructions
    ) : ProcessBox(inPorts, outPorts, std::move(instructions)),
        m_instructionPointer(0),
        m_waitCounter(0),
        m_state(State::Running)
    { }

    SequentialProcessBox::~SequentialProcessBox() = default;

    bool SequentialProcessBox::waitOnSatisfied()
    {
        const auto& wait = static_cast<const ProcessInstructionWaitOn&>(m_instructions[m_instructionPointer]);
        const ResolvedInstruction& resolved = m_resolved[m_instructionPointer];

        if (wait.hasTimeout && --m_waitCounter == 0)
            return true;

        const bool triggered = std::any_of(resolved.triggers.begin(), resolved.triggers.end(), &ProcessBox::conditionMet);
        return triggered && (!resolved.first || conditionMet(resolved.first));
    }

    void SequentialProcessBox::update()
    {
        switch (m_state)
        {
            case State::WaitingForever:
                return;

            case State::WaitingTime:
                if (--m_waitCounter != 0)
                    return; // Still waiting, do not proceed to the next instruction
                break;

            case State::WaitingOn:
                if (!waitOnSatisfied())
                    return;
                break;

            case State::Running:
                exec();
                return;
        }

        // Resuming after a wait: continue with the instruction that follows it.
        m_state = State::Running;
        m_instructionPointer++;
        exec();
    }

    void SequentialProcessBox::exec()
    {
        // The body restarts from the top after its last instruction. Wrapping twice in one run means no wait
        // instruction was reached, so the process would never suspend.
        size_t wraps = 0;

        while (true)
        {
            if (m_instructionPointer >= m_instructions.size())
            {
                m_instructionPointer = 0;
                if (++wraps == 2 || m_instructions.empty())
                    throw std::runtime_error("ProcessBox: Possible infinite loop detected. No wait instruction found in the process.");
            }

            const ProcessInstruction& instruction = m_instructions[m_instructionPointer];

            switch (instruction.kind)
            {
                case ProcessInstructionKind::Assignment:
                    assign(m_instructionPointer);
                    break;

                case ProcessInstructionKind::Branch:
                    if (!conditionMet(m_resolved[m_instructionPointer].first))
                        m_instructionPointer += static_cast<const ProcessInstructionBranch&>(instruction).branchLength; // Skip if false
                    break;

                case ProcessInstructionKind::BranchAlways:
                    m_instructionPointer += static_cast<const ProcessInstructionBranchAlways&>(instruction).branchLength; // Unconditionally skip
                    break;

                case ProcessInstructionKind::Wait:
                    if ((m_waitCounter = static_cast<const ProcessInstructionWait&>(instruction).waitTime) != 0)
                    {
                        m_state = State::WaitingTime;
                        return;
                    }
                    break;

                case ProcessInstructionKind::WaitForever:
                    m_state = State::WaitingForever;
                    return;

                case ProcessInstructionKind::WaitOn:
                {
                    const auto& wait = static_cast<const ProcessInstructionWaitOn&>(instruction);
                    m_waitCounter = wait.hasTimeout ? std::max<simTime_t>(wait.timeout, 1) : 0;
                    m_state = State::WaitingOn;
                    return;
                }
            }

            m_instructionPointer++;
        }
    }

    // --------------------------------------------------------------------------------------------

    CombinationalProcessBox::CombinationalProcessBox(
        const PortInitializer& inPorts,
        const PortInitializer& outPorts,
        ProcessProgram instructions,
        const std::vector<Wire*>& triggers
    ) : ProcessBox(inPorts, outPorts, std::move(instructions)),
        m_initialized(false)
    {
        for (auto* wire : triggers) if (wire)
            m_triggers.push_back(wire);
    }

    CombinationalProcessBox::~CombinationalProcessBox() = default;

    void CombinationalProcessBox::update()
    {
        const bool triggered = !m_initialized
            || std::any_of(m_triggers.begin(), m_triggers.end(), &ProcessBox::conditionMet);

        m_initialized = true;
        if (triggered)
            exec();
    }

    void CombinationalProcessBox::exec()
    {
        size_t instructionPointer = 0;
        while (instructionPointer < m_instructions.size())
        {
            const ProcessInstruction& instruction = m_instructions[instructionPointer];

            switch (instruction.kind)
            {
                case ProcessInstructionKind::Assignment:
                    assign(instructionPointer);
                    break;

                case ProcessInstructionKind::Branch:
                    if (!conditionMet(m_resolved[instructionPointer].first))
                        instructionPointer += static_cast<const ProcessInstructionBranch&>(instruction).branchLength; // Skip if false
                    break;

                case ProcessInstructionKind::BranchAlways:
                    instructionPointer += static_cast<const ProcessInstructionBranchAlways&>(instruction).branchLength; // Unconditionally skip
                    break;

                default:
                    throw std::runtime_error("ProcessBox: A process with a sensitivity list cannot contain a wait instruction.");
            }

            instructionPointer++;
        }
    }

} // namespace Pulse::Engine
