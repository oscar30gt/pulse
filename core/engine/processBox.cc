#include "processBox.h"

namespace Pulse::Engine
{
    ProcessBox::ProcessBox(
        const PortInitializer& inPorts,
        const PortInitializer& outPorts,
        std::vector<std::unique_ptr<ProcessInstruction>> instructions
    ) : Component(inPorts, outPorts),
        m_instructions(std::move(instructions))
    {
        for (const auto& [outportName, outWire] : outPorts) if (outWire)
        {
            outputSrcs.emplace(outportName, SignalSource(outWire->width()));

            try {
                outputSrcs.at(outportName).addTarget(outWire);
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error("ProcessBox construction failed: Unable to connect output port '" + outportName + "' to its wire. " + e.what());
            }
        }
    }

    ProcessBox::~ProcessBox() = default;

    // --------------------------------------------------------------------------------------------

    SequentialProcessBox::SequentialProcessBox(
        const PortInitializer& inPorts,
        const PortInitializer& outPorts,
        std::vector<std::unique_ptr<ProcessInstruction>> instructions
    ) : ProcessBox(inPorts, outPorts, std::move(instructions)),
        m_waitCounter(0),
        m_instructionPointer(0)
    { }

    SequentialProcessBox::~SequentialProcessBox() = default;

    void SequentialProcessBox::update()
    {
        if (m_waitCounter > 0)
        {
            m_waitCounter--;

            if (m_waitCounter != 0)
                return; // Still waiting, do not proceed to the next instruction

            m_instructionPointer++;
        }

        exec();
    }

    void SequentialProcessBox::exec()
    {
        uint8_t iterationCount = 0;

        while (true)
        {
            if (m_instructionPointer >= m_instructions.size()) m_instructionPointer = 0;
            auto* instPtr = m_instructions[m_instructionPointer].get();

            if (auto* assign = dynamic_cast<ProcessInstructionAssignment*>(instPtr))
            {
                auto value = getPort(assign->sourcePort)->peek();
                outputSrcs.at(assign->targetPort).drive(value);
            }

            else if (auto* branch = dynamic_cast<ProcessInstructionBranch*>(instPtr))
            {
                bool conditionValue = (bool)(getPort(branch->conditionPort)->peek());
                if (!conditionValue)
                {
                    m_instructionPointer += branch->branchLength; // Skip if false
                }
            }

            else if (auto* wait = dynamic_cast<ProcessInstructionWait*>(instPtr))
            {
                if ((m_waitCounter = wait->waitTime) != 0)
                    break; // <- Only exit. If no waits are found, the process will run indefinitely. (developer's responsibility to avoid infinite loops)
            }

            else if (auto* branchAlways = dynamic_cast<ProcessInstructionBranchAlways*>(instPtr))
            {
                m_instructionPointer += branchAlways->branchLength; // Unconditionally skip
            }

            m_instructionPointer++;

            if (++iterationCount > 10)
            {
                throw std::runtime_error("ProcessBox: Possible infinite loop detected. No wait instruction found in the process.");
            }
        }
    }

    // --------------------------------------------------------------------------------------------

    CombinationalProcessBox::CombinationalProcessBox(
        const PortInitializer& inPorts,
        const PortInitializer& outPorts,
        std::vector<std::unique_ptr<ProcessInstruction>> instructions,
        const std::vector<Wire*>& sensList
    ) : ProcessBox(inPorts, outPorts, std::move(instructions)),
        m_changed(false)
    {
        for (auto* wire : sensList) if (wire)
        {
            auto drain = std::make_unique<SignalDrain>(wire->width(), this, &CombinationalProcessBox::onSensitivityChange);
            drain->addSource(wire);
            m_sensitivityList.push_back(std::move(drain));
        }
    }

    CombinationalProcessBox::~CombinationalProcessBox() = default;

    bool CombinationalProcessBox::onSensitivityChange(ttl_t)
    {
        m_changed = true;
        return true;
    }

    void CombinationalProcessBox::update()
    {
        if (m_changed)
        {
            m_changed = false;
            exec();
        }
    }

    void CombinationalProcessBox::exec()
    {
        size_t instructionPointer = 0;
        while (instructionPointer < m_instructions.size())
        {
            auto* instPtr = m_instructions[instructionPointer].get();

            if (auto* assign = dynamic_cast<ProcessInstructionAssignment*>(instPtr))
            {
                auto value = getPort(assign->sourcePort)->peek();
                outputSrcs[assign->targetPort].drive(value);
            }

            else if (auto* branch = dynamic_cast<ProcessInstructionBranch*>(instPtr))
            {
                bool conditionValue = (bool)(getPort(branch->conditionPort)->peek());
                if (!conditionValue)
                {
                    instructionPointer += branch->branchLength; // Skip if false
                }
            }

            else if (auto* branchAlways = dynamic_cast<ProcessInstructionBranchAlways*>(instPtr))
            {
                instructionPointer += branchAlways->branchLength; // Unconditionally skip
            }

            instructionPointer++;
        }
    }

} // namespace Pulse::Engine