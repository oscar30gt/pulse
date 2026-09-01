#include "linker.h"
#include <iostream>
#include <string>
#include <vector>

namespace Pulse::Parser
{
    namespace
    {
        // ANSI Color palette
        namespace Color
        {
            constexpr const char* Reset     = "\033[0m";
            constexpr const char* Header    = "\033[1;35m"; // Magenta
            constexpr const char* TreeLine  = "\033[90m";   // Gray
            constexpr const char* Root      = "\033[1;36m"; // Bold Cyan
            constexpr const char* Entity    = "\033[1;33m"; // Bold Yellow
            constexpr const char* Arch      = "\033[1;32m"; // Bold Green
            constexpr const char* Section   = "\033[36m";   // Cyan
            constexpr const char* Node      = "\033[1;34m"; // Bold Blue
            constexpr const char* Label     = "\033[90m";   // Dark Gray
            constexpr const char* Literal   = "\033[33m";   // Yellow
            constexpr const char* Type      = "\033[35m";   // Magenta
        }

        void printIndent(const std::vector<bool>& isLastStack)
        {
            for (size_t i = 0; i < isLastStack.size(); ++i)
            {
                if (i == isLastStack.size() - 1)
                {
                    std::cout << Color::TreeLine << "+-- " << Color::Reset;
                }
                else
                {
                    std::cout << Color::TreeLine << (isLastStack[i] ? "    " : "|   ") << Color::Reset;
                }
            }
        }

        void printASTNode(const ASTNode* node, std::vector<bool>& isLastStack);

        void printASTNode(const ASTNode* node, std::vector<bool>& isLastStack)
        {
            if (!node)
            {
                return;
            }

            // SignalReference formatting
            if (auto sigRef = dynamic_cast<const SignalReference*>(node))
            {
                printIndent(isLastStack);
                if (!sigRef->low && !sigRef->high)
                {
                    std::cout << Color::Node << "SignalReference: " << Color::Reset 
                              << sigRef->signalName << " " << Color::Label << "(full signal)" << Color::Reset << "\n";
                }
                else if (sigRef->low && !sigRef->high)
                {
                    std::cout << Color::Node << "SignalReference: " << Color::Reset 
                              << sigRef->signalName << " " << Color::Label << "(index)" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printIndent(isLastStack);
                    std::cout << Color::Label << "index:" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(sigRef->low.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();
                }
                else
                {
                    std::cout << Color::Node << "SignalReference: " << Color::Reset 
                              << sigRef->signalName << " " << Color::Label << "(range)" << Color::Reset << "\n";
                    isLastStack.push_back(false);
                    printIndent(isLastStack);
                    std::cout << Color::Label << "low:" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(sigRef->low.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();

                    isLastStack.push_back(true);
                    printIndent(isLastStack);
                    std::cout << Color::Label << "high:" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(sigRef->high.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();
                }
            }
            // IntegerLiteralExpr formatting
            else if (auto intLit = dynamic_cast<const IntegerLiteralExpr*>(node))
            {
                printIndent(isLastStack);
                std::cout << Color::Node << "IntegerLiteral: " << Color::Literal << std::to_string(intLit->value) << Color::Reset << "\n";
            }
            // LogicLiteralExpr formatting
            else if (auto logicLit = dynamic_cast<const LogicLiteralExpr*>(node))
            {
                printIndent(isLastStack);
                std::cout << Color::Node << "LogicLiteral: " << Color::Reset 
                          << Color::Label << "value=" << Color::Literal << "0x" << std::hex << logicLit->value 
                          << Color::Label << " mask=" << Color::Literal << "0x" << logicLit->unknownMask << std::dec 
                          << Color::Label << " width=" << Color::Literal << std::to_string(logicLit->width) << Color::Reset << "\n";
            }
            // BinaryOpExpr (LOGIC or BOOLEAN)
            else if (auto binOpLogic = dynamic_cast<const BinaryOpExpr<ReturnType::LOGIC>*>(node))
            {
                printIndent(isLastStack);
                std::cout << Color::Node << "BinaryOp" << Color::Type << "[LOGIC]" << Color::Reset << ": " << binOpLogic->op << "\n";
                
                isLastStack.push_back(false);
                printIndent(isLastStack);
                std::cout << Color::Label << "left:" << Color::Reset << "\n";
                isLastStack.push_back(true);
                printASTNode(binOpLogic->left.get(), isLastStack);
                isLastStack.pop_back();
                isLastStack.pop_back();

                isLastStack.push_back(true);
                printIndent(isLastStack);
                std::cout << Color::Label << "right:" << Color::Reset << "\n";
                isLastStack.push_back(true);
                printASTNode(binOpLogic->right.get(), isLastStack);
                isLastStack.pop_back();
                isLastStack.pop_back();
            }
            else if (auto binOpBool = dynamic_cast<const BinaryOpExpr<ReturnType::BOOLEAN>*>(node))
            {
                printIndent(isLastStack);
                std::cout << Color::Node << "BinaryOp" << Color::Type << "[BOOLEAN]" << Color::Reset << ": " << binOpBool->op << "\n";

                isLastStack.push_back(false);
                printIndent(isLastStack);
                std::cout << Color::Label << "left:" << Color::Reset << "\n";
                isLastStack.push_back(true);
                printASTNode(binOpBool->left.get(), isLastStack);
                isLastStack.pop_back();
                isLastStack.pop_back();

                isLastStack.push_back(true);
                printIndent(isLastStack);
                std::cout << Color::Label << "right:" << Color::Reset << "\n";
                isLastStack.push_back(true);
                printASTNode(binOpBool->right.get(), isLastStack);
                isLastStack.pop_back();
                isLastStack.pop_back();
            }
            // WhenElseExpr formatting
            else if (auto whenElse = dynamic_cast<const WhenElseExpr*>(node))
            {
                printIndent(isLastStack);
                std::cout << Color::Node << "WhenElse " << Color::Label << "(" << std::to_string(whenElse->branches.size()) << " branches)" << Color::Reset << "\n";

                size_t totalElements = whenElse->branches.size() + (whenElse->defaultValue ? 1 : 0);
                size_t currentIndex = 0;

                for (const auto& branch : whenElse->branches)
                {
                    currentIndex++;
                    bool isLastBranch = (currentIndex == totalElements);

                    isLastStack.push_back(isLastBranch);
                    printIndent(isLastStack);
                    std::cout << Color::Node << "when #" << std::to_string(currentIndex) << Color::Reset << "\n";

                    isLastStack.push_back(false);
                    printIndent(isLastStack);
                    std::cout << Color::Label << "condition:" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(branch.condition.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();

                    isLastStack.push_back(true);
                    printIndent(isLastStack);
                    std::cout << Color::Label << "value:" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(branch.value.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();

                    isLastStack.pop_back();
                }

                if (whenElse->defaultValue)
                {
                    isLastStack.push_back(true);
                    printIndent(isLastStack);
                    std::cout << Color::Node << "else" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(whenElse->defaultValue.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();
                }
            }
        }
    } // namespace

    void Linker::printLinkedDesign(const LinkedDesign& design)
    {
        std::cout << Color::Header << "============================================================\n";
        std::cout << "VHDL Linked Design Tree\n";
        std::cout << "============================================================\n\n" << Color::Reset;
        std::cout << Color::Root << "ROOT" << Color::Reset << "\n";

        std::vector<bool> isLastStack;

        size_t totalEntities = design.entities.size();
        size_t totalArchitectures = design.architectures.size();
        size_t totalTopNodes = totalEntities + totalArchitectures;
        size_t currentTopNode = 0;

        // Print Entities
        for (const auto& [entityName, entityPtr] : design.entities)
        {
            currentTopNode++;
            bool isLastTop = (currentTopNode == totalTopNodes);

            isLastStack.push_back(isLastTop);
            printIndent(isLastStack);
            std::cout << Color::Entity << "ENTITY: " << Color::Reset << entityName << "\n";

            if (entityPtr && !entityPtr->ports.empty())
            {
                isLastStack.push_back(true);
                printIndent(isLastStack);
                std::cout << Color::Section << "Ports " << Color::Label << "(" << std::to_string(entityPtr->ports.size()) << ")" << Color::Reset << "\n";

                for (size_t i = 0; i < entityPtr->ports.size(); ++i)
                {
                    bool isLastPort = (i == entityPtr->ports.size() - 1);
                    const auto& port = entityPtr->ports[i];

                    isLastStack.push_back(isLastPort);
                    printIndent(isLastStack);
                    std::cout << Color::Node << "PORT: " << Color::Reset << port->portName 
                              << " [" << (port->isInput ? "IN" : "OUT") << "]"
                              << Color::Label << " width=" << Color::Literal << std::to_string(port->width) << Color::Reset << "\n";
                    isLastStack.pop_back();
                }
                isLastStack.pop_back();
            }
            isLastStack.pop_back();
        }

        // Print Architectures
        for (const auto& arch : design.architectures)
        {
            currentTopNode++;
            bool isLastTop = (currentTopNode == totalTopNodes);

            isLastStack.push_back(isLastTop);
            printIndent(isLastStack);
            std::cout << Color::Arch << "ARCHITECTURE: " << Color::Reset << arch.architectureName 
                      << " OF " << (arch.targetEntity ? arch.targetEntity->entityName : "<unresolved>") << "\n";

            size_t totalSections = (!arch.signals.empty() ? 1 : 0) + 
                                   (!arch.assignments.empty() ? 1 : 0) + 
                                   (!arch.resolvedInstantiations.empty() ? 1 : 0) +
                                   (!arch.processes.empty() ? 1 : 0);
            size_t currentSection = 0;

            // Signals Section
            if (!arch.signals.empty())
            {
                currentSection++;
                bool isLastSec = (currentSection == totalSections);

                isLastStack.push_back(isLastSec);
                printIndent(isLastStack);
                std::cout << Color::Section << "Signals " << Color::Label << "(" << std::to_string(arch.signals.size()) << ")" << Color::Reset << "\n";

                for (size_t i = 0; i < arch.signals.size(); ++i)
                {
                    bool isLastSig = (i == arch.signals.size() - 1);
                    const auto* sig = arch.signals[i];

                    isLastStack.push_back(isLastSig);
                    printIndent(isLastStack);
                    std::cout << Color::Node << "SIGNAL: " << Color::Reset << sig->signalName 
                              << Color::Label << " width=" << Color::Literal << std::to_string(sig->width)
                              << Color::Label << " init=" << Color::Literal << "0x" << std::hex << sig->initialValue << std::dec << Color::Reset << "\n";
                    isLastStack.pop_back();
                }
                isLastStack.pop_back();
            }

            // Assignments Section
            if (!arch.assignments.empty())
            {
                currentSection++;
                bool isLastSec = (currentSection == totalSections);

                isLastStack.push_back(isLastSec);
                printIndent(isLastStack);
                std::cout << Color::Section << "Assignments " << Color::Label << "(" << std::to_string(arch.assignments.size()) << ")" << Color::Reset << "\n";

                for (size_t i = 0; i < arch.assignments.size(); ++i)
                {
                    bool isLastAssign = (i == arch.assignments.size() - 1);
                    const auto* assign = arch.assignments[i];

                    isLastStack.push_back(isLastAssign);
                    printIndent(isLastStack);
                    std::cout << Color::Node << "ASSIGNMENT" << Color::Reset << "\n";

                    // Target subtree
                    isLastStack.push_back(false);
                    printIndent(isLastStack);
                    std::cout << Color::Label << "target:" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(assign->target.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();

                    // Value subtree
                    isLastStack.push_back(true);
                    printIndent(isLastStack);
                    std::cout << Color::Label << "value:" << Color::Reset << "\n";
                    isLastStack.push_back(true);
                    printASTNode(assign->value.get(), isLastStack);
                    isLastStack.pop_back();
                    isLastStack.pop_back();

                    isLastStack.pop_back();
                }
                isLastStack.pop_back();
            }

            // Resolved Instantiations Section
            if (!arch.resolvedInstantiations.empty())
            {
                currentSection++;
                bool isLastSec = (currentSection == totalSections);

                isLastStack.push_back(isLastSec);
                printIndent(isLastStack);
                std::cout << Color::Section << "Instantiations " << Color::Label << "(" << std::to_string(arch.resolvedInstantiations.size()) << ")" << Color::Reset << "\n";

                for (size_t i = 0; i < arch.resolvedInstantiations.size(); ++i)
                {
                    bool isLastInst = (i == arch.resolvedInstantiations.size() - 1);
                    const auto& inst = arch.resolvedInstantiations[i];

                    isLastStack.push_back(isLastInst);
                    printIndent(isLastStack);
                    std::cout << Color::Node << "INSTANCE: " << Color::Reset << inst.instanceName 
                              << " -> ENTITY " << (inst.targetEntity ? inst.targetEntity->entityName : "<unresolved>") << "\n";

                    if (!inst.portBindings.empty())
                    {
                        isLastStack.push_back(true);
                        printIndent(isLastStack);
                        std::cout << Color::Section << "Port Bindings " << Color::Label << "(" << std::to_string(inst.portBindings.size()) << ")" << Color::Reset << "\n";

                        size_t portIdx = 0;
                        for (const auto& [portName, boundNode] : inst.portBindings)
                        {
                            portIdx++;
                            bool isLastBinding = (portIdx == inst.portBindings.size());

                            isLastStack.push_back(isLastBinding);
                            printIndent(isLastStack);
                            std::cout << Color::Node << "BINDING: " << Color::Reset << portName << " =>\n";

                            isLastStack.push_back(true);
                            printASTNode(boundNode, isLastStack);
                            isLastStack.pop_back();

                            isLastStack.pop_back();
                        }
                        isLastStack.pop_back();
                    }
                    isLastStack.pop_back();
                }
                isLastStack.pop_back();
            }

            // Processes Section
            if (!arch.processes.empty())
            {
                currentSection++;
                bool isLastSec = (currentSection == totalSections);

                isLastStack.push_back(isLastSec);
                printIndent(isLastStack);
                std::cout << Color::Section << "Processes " << Color::Label << "(" << std::to_string(arch.processes.size()) << ")" << Color::Reset << "\n";

                for (size_t i = 0; i < arch.processes.size(); ++i)
                {
                    bool isLastProc = (i == arch.processes.size() - 1);
                    const auto* proc = arch.processes[i];

                    isLastStack.push_back(isLastProc);
                    printIndent(isLastStack);
                    std::cout << Color::Node << "PROCESS: " << Color::Reset << (proc->label.empty() ? "<unlabeled>" : proc->label) << "\n";
                    isLastStack.pop_back();
                }
                isLastStack.pop_back();
            }

            isLastStack.pop_back();
        }
    }

} // namespace Pulse::Parser