#ifndef PULSE_PROCESS_BOX_H
#define PULSE_PROCESS_BOX_H

#include <memory>
#include <vector>

#include "component.h"
#include "signalDrain.h"
#include "signalSource.h"

namespace Pulse::Engine
{
    struct ProcessInstruction { virtual ~ProcessInstruction() = default; };

    // Assigns a logic value to some of the process outputs.
    struct ProcessInstructionAssignment : public ProcessInstruction
    {
        std::string targetPort;
        std::string sourcePort;
    };

    /// Branches if a condition is met. Otherwise, branch is skipped.
    struct ProcessInstructionBranch : public ProcessInstruction
    {
        std::string conditionPort;
        size_t branchLength; /// Amount of instructions to skip if condition is false (conditionPort is 0)
    };

    struct ProcessInstructionBranchAlways : public ProcessInstruction
    {
        size_t branchLength; /// Amount of instructions to skip unconditionally
    };

    // --------------------------------------------------------------------------------------------

    /// Component that simulates a VHDL-like process block, executing a sequence of instructions in order.
    class ProcessBox : public Component
    {

        virtual void exec() = 0;

    protected:

        /// Set of instructions to be executed.
        const std::vector<std::unique_ptr<ProcessInstruction>> m_instructions;

        /// Interfaces to drive values to the output wires.
        std::unordered_map<std::string, SignalSource> outputSrcs;

    public:
        ProcessBox(
            const PortInitializer& inPorts,
            const PortInitializer& outPorts,
            std::vector<std::unique_ptr<ProcessInstruction>> instructions
        );
        virtual ~ProcessBox() override;
    };

    // --------------------------------------------------------------------------------------------

    /// Wait instruction. Will pause the processBox for a certain amount of time 
    /// before proceeding to the next instruction. Can only be used with SequentialProcessBox.
    struct ProcessInstructionWait : public ProcessInstruction
    {
        uint64_t waitTime; /// in femtoseconds (max before overflow: around 5 hours)
    };

    /// Wait forever instruction. Will pause the processBox indefinitely.
    struct ProcessInstructionWaitForever : public ProcessInstruction
    {
        // No additional members needed; the presence of this instruction indicates an indefinite wait.
    };

    /// A sequential process executing instructions in order, with the ability to wait
    /// for a certain amount of time or until a signal changes before proceeding to the next instruction.
    class SequentialProcessBox : public ProcessBox
    {
        /// Pointer to the instruction to be executed next.
        size_t m_instructionPointer;

        /// Counter for the wait instruction.
        uint64_t m_waitCounter;

        /// Flag indicating if the process is currently in a wait-forever state.
        bool m_waitingForever;

        /// Continues the execution of the process from the current instruction pointer
        /// until a wait instruction is encountered.
        virtual void exec() override;

    public:
        SequentialProcessBox(
            const PortInitializer& inPorts,
            const PortInitializer& outPorts,
            std::vector<std::unique_ptr<ProcessInstruction>> instructions
        );
        virtual ~SequentialProcessBox() override;

        virtual void update() override;
    };

    /// A combinational process executing instructions in order whenever a sensi
    class CombinationalProcessBox : public ProcessBox
    {
        std::vector<std::unique_ptr<SignalDrain>> m_sensitivityList;
        bool m_changed;

        /// Handles the notification from the sensitivity list, marking the process
        /// as changed and ready to be executed in the next update cycle.
        bool onSensitivityChange(ttl_t ttl = TTL_DEFAULT);

        /// Executes the process instructions from the start.
        virtual void exec() override;

    public:
        CombinationalProcessBox(
            const PortInitializer& inPorts,
            const PortInitializer& outPorts,
            std::vector<std::unique_ptr<ProcessInstruction>> instructions,
            const std::vector<Wire*>& sensList = std::vector<Wire*>()
        );
        virtual ~CombinationalProcessBox() override;

        virtual void update() override;
    };

} // namespace Pulse::Engine

#endif // PULSE_PROCESS_BOX_H