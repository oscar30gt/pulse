#ifndef PULSE_PROCESS_BOX_H
#define PULSE_PROCESS_BOX_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "component.h"

namespace Pulse::Engine
{
    /// Kind of a process instruction, so the process box can dispatch on it statically.
    enum class ProcessInstructionKind : uint8_t
    {
        Assignment,
        Branch,
        BranchAlways,
        Wait,
        WaitForever,
        WaitOn,
    };

    /// Base of every process instruction. Each instruction type sets its kind once, when it is constructed.
    struct ProcessInstruction
    {
        const ProcessInstructionKind kind;
        virtual ~ProcessInstruction() = default;

    protected:
        explicit ProcessInstruction(ProcessInstructionKind kind) : kind(kind) { }
    };

    /// Assigns the value of a port to another port (the target wire must not have sources: it is driven directly).
    struct ProcessInstructionAssignment : public ProcessInstruction
    {
        std::string targetPort;
        std::string sourcePort;
        /// A signal assignment: the value is applied when every process of the tick has run (commit()), as VHDL applies
        /// signal assignments after the processes suspend. Otherwise (variables) the target changes immediately.
        bool deferred = false;

        ProcessInstructionAssignment() : ProcessInstruction(ProcessInstructionKind::Assignment) { }
    };

    /// Branches if a condition is met. Otherwise, branch is skipped.
    /// The condition is met only when bit 0 of the condition port is a definite '1' ('X' and 'Z' count as false).
    struct ProcessInstructionBranch : public ProcessInstruction
    {
        std::string conditionPort;
        size_t branchLength = 0; /// Amount of instructions to skip if the condition is not met

        ProcessInstructionBranch() : ProcessInstruction(ProcessInstructionKind::Branch) { }
    };

    struct ProcessInstructionBranchAlways : public ProcessInstruction
    {
        size_t branchLength = 0; /// Amount of instructions to skip unconditionally

        ProcessInstructionBranchAlways() : ProcessInstruction(ProcessInstructionKind::BranchAlways) { }
    };

    /// Wait instruction. Will pause the processBox for a certain amount of time
    /// before proceeding to the next instruction. Can only be used with SequentialProcessBox.
    struct ProcessInstructionWait : public ProcessInstruction
    {
        simTime_t waitTime = 0; /// in femtoseconds (max before overflow: around 5 hours)

        ProcessInstructionWait() : ProcessInstruction(ProcessInstructionKind::Wait) { }
    };

    /// Wait forever instruction. Will pause the processBox indefinitely.
    struct ProcessInstructionWaitForever : public ProcessInstruction
    {
        ProcessInstructionWaitForever() : ProcessInstruction(ProcessInstructionKind::WaitForever) { }
    };

    /// VHDL `wait on <signals> until <condition> for <timeout>`, any part being optional.
    /// The process resumes on the first tick where a trigger is a definite '1' and the condition (if any) is a definite
    /// '1', or once the timeout (if any) has elapsed. The triggers are 1-bit wires, normally the outputs of the EventProbes
    /// of the signals waited on. Can only be used with SequentialProcessBox.
    struct ProcessInstructionWaitOn : public ProcessInstruction
    {
        std::vector<std::string> triggers;      /// Ports of the 1-bit wires that may resume the process
        std::string conditionPort;              /// Condition checked on an event; empty when there is none
        simTime_t timeout = 0;                  /// Maximum wait in femtoseconds, when hasTimeout
        bool hasTimeout = false;

        ProcessInstructionWaitOn() : ProcessInstruction(ProcessInstructionKind::WaitOn) { }
    };

    // --------------------------------------------------------------------------------------------

    /// The instructions of a process. They are immutable and shared, so every instance of a blueprint runs the same
    /// program without copying it.
    class ProcessProgram
    {
        std::shared_ptr<const std::vector<std::unique_ptr<ProcessInstruction>>> m_instructions;

    public:
        /// An empty program.
        ProcessProgram();
        /// Takes ownership of the instructions.
        ProcessProgram(std::vector<std::unique_ptr<ProcessInstruction>> instructions);

        [[nodiscard]] size_t size() const;
        [[nodiscard]] bool empty() const;
        [[nodiscard]] const ProcessInstruction& operator[](size_t index) const;
    };

    // --------------------------------------------------------------------------------------------

    /// Component that simulates a VHDL-like process block, executing a sequence of instructions in order.
    /// Assignments drive their target wires directly (Wire::drive), so a wire assigned by a process must have no sources.
    class ProcessBox : public Component
    {
        virtual void exec() = 0;

    protected:

        /// Wires an instruction refers to, resolved once when the process is built.
        struct ResolvedInstruction
        {
            Wire* first = nullptr;              ///< Assignment target, branch condition, wait condition
            Wire* second = nullptr;             ///< Assignment source
            std::vector<Wire*> triggers;        ///< Wait triggers
        };

        /// Set of instructions to be executed.
        const ProcessProgram m_instructions;

        /// Wires of each instruction, parallel to m_instructions.
        std::vector<ResolvedInstruction> m_resolved;

        /// Deferred signal assignments waiting for commit(), in order; a wire appears at most once.
        std::vector<std::pair<Wire*, LogicVector>> m_pending;

        /// Executes an assignment: drives the target now, or queues the value when it is deferred.
        void assign(size_t index);

        /// Whether the condition wire holds a definite '1' in bit 0.
        static bool conditionMet(const Wire* condition);

    public:
        ProcessBox(
            const PortInitializer& inPorts,
            const PortInitializer& outPorts,
            ProcessProgram instructions
        );
        virtual ~ProcessBox() override;

        /// Applies the deferred signal assignments of the last run.
        virtual void commit() override;
    };

    // --------------------------------------------------------------------------------------------

    /// A sequential process executing instructions in order, with the ability to wait
    /// for a certain amount of time or until a signal changes before proceeding to the next instruction.
    /// It runs on its first update (VHDL initialization) and restarts from the top after its last instruction.
    class SequentialProcessBox : public ProcessBox
    {
        /// What the process is doing between two updates.
        enum class State : uint8_t { Running, WaitingTime, WaitingOn, WaitingForever };

        /// Pointer to the instruction to be executed next (the wait instruction while waiting).
        size_t m_instructionPointer;

        /// Counter for the wait instructions.
        simTime_t m_waitCounter;

        State m_state;

        /// Continues the execution of the process from the current instruction pointer
        /// until a wait instruction is encountered.
        virtual void exec() override;

        /// Whether the `wait on/until/for` the process is suspended at lets it resume on this tick.
        bool waitOnSatisfied();

    public:
        SequentialProcessBox(
            const PortInitializer& inPorts,
            const PortInitializer& outPorts,
            ProcessProgram instructions
        );
        virtual ~SequentialProcessBox() override;

        virtual void update() override;
    };

    /// A process with a sensitivity list: it runs once on its first update (VHDL initialization) and then, from the
    /// start, on every update where one of its triggers is a definite '1'. The triggers are 1-bit wires, normally the
    /// outputs of the EventProbes of the signals in the sensitivity list.
    class CombinationalProcessBox : public ProcessBox
    {
        std::vector<Wire*> m_triggers;
        bool m_initialized;

        /// Executes the process instructions from the start.
        virtual void exec() override;

    public:
        CombinationalProcessBox(
            const PortInitializer& inPorts,
            const PortInitializer& outPorts,
            ProcessProgram instructions,
            const std::vector<Wire*>& triggers = std::vector<Wire*>()
        );
        virtual ~CombinationalProcessBox() override;

        virtual void update() override;
    };

} // namespace Pulse::Engine

#endif // PULSE_PROCESS_BOX_H
