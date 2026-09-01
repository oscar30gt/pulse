#include "AST.h"
#include <iostream>
#include <string>
#include <memory>
#include <iomanip>

namespace Pulse::Parser
{
    namespace
    {
        // ANSI Color Escape Sequences
        constexpr const char* RESET   = "\033[0m";
        constexpr const char* DIM     = "\033[90m";   // Subtle dark gray for tree lines & nulls
        constexpr const char* KEYWORD = "\033[1;36m"; // Bold cyan for node types and labels
        constexpr const char* IDENT   = "\033[32m";   // Green for identifiers and names
        constexpr const char* VALUE   = "\033[33m";   // Yellow for values, counts, and numbers
        constexpr const char* TYPE    = "\033[35m";   // Magenta for directions ([IN]/[OUT]) & return types

        // ASCII characters with embedded DIM styling to preserve dimming across nested levels
        constexpr const char* TREE_TEE   = "\033[90m+-- \033[0m";
        constexpr const char* TREE_LAST  = "\033[90m+-- \033[0m";
        constexpr const char* TREE_VERT  = "\033[90m|   \033[0m";
        constexpr const char* TREE_SPACE = "    ";

        // Helper to print colored tree branches consistently
        inline void printBranch(const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE);
        }

        // Forward declarations for helper functions
        void printNode(const ASTNode* node, const std::string& prefix, bool isLast);
        void printExpression(const ASTNode* expr, const std::string& prefix, bool isLast);
        void printSignalReference(const SignalReference* ref, const std::string& prefix, bool isLast);
        void printSequentialStatement(const SequentialStatement* stmt, const std::string& prefix, bool isLast);
        void printProcessStatement(const ProcessStatement* proc, const std::string& prefix, bool isLast);
        void printIfStatement(const IfStatement* ifStmt, const std::string& prefix, bool isLast);
        void printWaitForStatement(const WaitForStatement* wait, const std::string& prefix, bool isLast);
        void printWaitForeverStatement(const WaitForeverStatement* wait, const std::string& prefix, bool isLast);

        void printPortDeclaration(const PortDeclaration* port,
            const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "PORT: " << IDENT << port->portName << " "
                << TYPE << "[" << (port->isInput ? "IN" : "OUT") << "] "
                << RESET << "width=" << VALUE << static_cast<int>(port->width) << RESET << "\n";
        }

        void printSignalDeclaration(const SignalDeclaration* signal,
            const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "SIGNAL: " << IDENT << signal->signalName
                << RESET << " width=" << VALUE << static_cast<int>(signal->width) << RESET;

            if (signal->initialValue != 0)
            {
                std::cout << " init=" << VALUE << "0x" << std::hex << signal->initialValue << std::dec << RESET;
            }
            std::cout << "\n";
        }

        void printEntityDeclaration(const EntityDeclaration* entity,
            const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "ENTITY: " << IDENT << entity->entityName << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            if (!entity->ports.empty())
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Ports (" << VALUE << entity->ports.size() << KEYWORD << ")" << RESET << "\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;

                for (size_t i = 0; i < entity->ports.size(); ++i)
                {
                    bool lastPort = (i == entity->ports.size() - 1);
                    printPortDeclaration(entity->ports[i].get(), portPrefix, lastPort);
                }
            }
        }

        void printComponentDeclaration(const ComponentDeclaration* comp,
            const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "COMPONENT: " << IDENT << comp->componentName << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            if (!comp->ports.empty())
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Ports (" << VALUE << comp->ports.size() << KEYWORD << ")" << RESET << "\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;

                for (size_t i = 0; i < comp->ports.size(); ++i)
                {
                    bool lastPort = (i == comp->ports.size() - 1);
                    printPortDeclaration(comp->ports[i].get(), portPrefix, lastPort);
                }
            }
        }

        std::string returnTypeToString(ReturnType rt)
        {
            switch (rt)
            {
                case ReturnType::LOGIC: return "LOGIC";
                case ReturnType::INTEGER: return "INTEGER";
                case ReturnType::SIGNED: return "SIGNED";
                case ReturnType::UNSIGNED: return "UNSIGNED";
                case ReturnType::BOOLEAN: return "BOOLEAN";
            }
            return "UNKNOWN";
        }

        void printSignalReference(const SignalReference* ref, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            if (!ref)
            {
                std::cout << DIM << "<null SignalReference>" << RESET << "\n";
                return;
            }

            std::string indexDesc;
            if (!ref->low && !ref->high)
                indexDesc = " (full signal)";
            else if (ref->low && !ref->high)
                indexDesc = " (single bit)";
            else
                indexDesc = " (range)";

            std::cout << KEYWORD << "SignalReference: " << IDENT << ref->signalName
                << DIM << indexDesc << RESET << "\n";

            if (ref->low || ref->high)
            {
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

                if (ref->low)
                {
                    bool lastBound = !ref->high;
                    printBranch(nextPrefix, lastBound);
                    std::cout << KEYWORD << "low:" << RESET << "\n";
                    printExpression(ref->low.get(), nextPrefix + (lastBound ? TREE_SPACE : TREE_VERT), true);
                }

                if (ref->high)
                {
                    printBranch(nextPrefix, true);
                    std::cout << KEYWORD << "high:" << RESET << "\n";
                    printExpression(ref->high.get(), nextPrefix + TREE_SPACE, true);
                }
            }
        }

        void printComponentInstantiation(const ComponentInstantiation* inst,
            const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "INSTANTIATION: " << IDENT << inst->instanceName
                << KEYWORD << " OF " << IDENT << inst->componentName << RESET << "\n";

            if (!inst->portMaps.empty())
            {
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Port Maps (" << VALUE << inst->portMaps.size() << KEYWORD << ")" << RESET << "\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;

                size_t idx = 0;
                for (const auto& [formalName, actualRef] : inst->portMaps)
                {
                    bool lastMap = (++idx == inst->portMaps.size());
                    printBranch(portPrefix, lastMap);
                    std::cout << KEYWORD << "formal: " << IDENT << formalName << RESET << "\n";
                    printSignalReference(actualRef.get(), portPrefix + (lastMap ? TREE_SPACE : TREE_VERT), true);
                }
            }
        }

        void printSignalAssignment(const SignalAssignment* assign,
            const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "ASSIGNMENT" << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            // Print Target SignalReference
            bool hasValue = (assign->value != nullptr);
            printBranch(nextPrefix, !hasValue);
            std::cout << KEYWORD << "target:" << RESET << "\n";
            printSignalReference(assign->target.get(), nextPrefix + (hasValue ? TREE_VERT : TREE_SPACE), true);

            // Print Value (could be WhenElseExpr or simple expression)
            if (hasValue)
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "value:" << RESET << "\n";
                printExpression(assign->value.get(), nextPrefix + TREE_SPACE, true);
            }
        }

        void printWaitForStatement(const WaitForStatement* wait, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "WAIT FOR: " << VALUE << wait->durationFs << RESET << " fs\n";
        }

        void printWaitForeverStatement(const WaitForeverStatement* wait, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "WAIT FOREVER" << RESET << "\n";
        }

        void printIfStatement(const IfStatement* ifStmt, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "IF (" << VALUE << ifStmt->branches.size()
                << KEYWORD << " branch" << (ifStmt->branches.size() != 1 ? "es" : "")
                << (ifStmt->elseBody.empty() ? "" : " + else") << ")" << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            for (size_t i = 0; i < ifStmt->branches.size(); ++i)
            {
                const auto& branch = ifStmt->branches[i];
                bool isLastBranch = (i == ifStmt->branches.size() - 1) && ifStmt->elseBody.empty();

                printBranch(nextPrefix, isLastBranch);
                std::cout << KEYWORD << (i == 0 ? "if" : "elsif") << ":" << RESET << "\n";
                std::string branchPrefix = nextPrefix + (isLastBranch ? TREE_SPACE : TREE_VERT);

                printBranch(branchPrefix, false);
                std::cout << KEYWORD << "condition:" << RESET << "\n";
                printExpression(branch.condition.get(), branchPrefix + TREE_VERT, true);

                bool hasBody = !branch.body.empty();
                if (hasBody)
                {
                    printBranch(branchPrefix, true);
                    std::cout << KEYWORD << "body:" << RESET << "\n";
                    std::string bodyPrefix = branchPrefix + TREE_SPACE;
                    for (size_t j = 0; j < branch.body.size(); ++j)
                        printSequentialStatement(branch.body[j].get(), bodyPrefix, j == branch.body.size() - 1);
                }
            }

            if (!ifStmt->elseBody.empty())
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "else:" << RESET << "\n";
                std::string elsePrefix = nextPrefix + TREE_SPACE;
                for (size_t j = 0; j < ifStmt->elseBody.size(); ++j)
                    printSequentialStatement(ifStmt->elseBody[j].get(), elsePrefix, j == ifStmt->elseBody.size() - 1);
            }
        }

        void printSequentialStatement(const SequentialStatement* stmt, const std::string& prefix, bool isLast)
        {
            if (!stmt)
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<null statement>" << RESET << "\n";
                return;
            }

            if (auto assign = dynamic_cast<const SignalAssignment*>(stmt))
                printSignalAssignment(assign, prefix, isLast);
            else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt))
                printIfStatement(ifStmt, prefix, isLast);
            else if (auto wait = dynamic_cast<const WaitForStatement*>(stmt))
                printWaitForStatement(wait, prefix, isLast);
            else if (auto waitForever = dynamic_cast<const WaitForeverStatement*>(stmt))
                printWaitForeverStatement(waitForever, prefix, isLast);
            else
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<Unknown SequentialStatement>" << RESET << "\n";
            }
        }

        void printProcessStatement(const ProcessStatement* proc, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "PROCESS";
            if (!proc->label.empty())
                std::cout << ": " << IDENT << proc->label;
            std::cout << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            // Sensitivity list
            bool hasBody = !proc->body.empty();
            if (!proc->sensitivityList.empty())
            {
                printBranch(nextPrefix, !hasBody);
                std::cout << KEYWORD << "sensitivity (" << VALUE << proc->sensitivityList.size() << KEYWORD << "):" << RESET;
                for (const auto& sig : proc->sensitivityList)
                    std::cout << " " << IDENT << sig << RESET;
                std::cout << "\n";
            }

            // Body statements
            if (hasBody)
            {
                for (size_t i = 0; i < proc->body.size(); ++i)
                    printSequentialStatement(proc->body[i].get(), nextPrefix, i == proc->body.size() - 1);
            }
        }

        void printArchitectureDeclaration(const ArchitectureDeclaration* arch,
            const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "ARCHITECTURE: " << IDENT << arch->architectureName
                << KEYWORD << " OF " << IDENT << arch->entityName << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            bool hasSignals = !arch->signals.empty();
            bool hasComponents = !arch->components.empty();
            bool hasInstantiations = !arch->instantiations.empty();
            bool hasAssignments = !arch->assignments.empty();
            bool hasProcesses = !arch->processes.empty();

            // Signals
            if (hasSignals)
            {
                bool isLastSection = !hasComponents && !hasInstantiations && !hasAssignments && !hasProcesses;
                printBranch(nextPrefix, isLastSection);
                std::cout << KEYWORD << "Signals (" << VALUE << arch->signals.size() << KEYWORD << ")" << RESET << "\n";
                std::string sigPrefix = nextPrefix + (isLastSection ? TREE_SPACE : TREE_VERT);

                for (size_t i = 0; i < arch->signals.size(); ++i)
                {
                    bool lastSignal = (i == arch->signals.size() - 1);
                    printSignalDeclaration(arch->signals[i].get(), sigPrefix, lastSignal);
                }
            }

            // Components
            if (hasComponents)
            {
                bool isLastSection = !hasInstantiations && !hasAssignments && !hasProcesses;
                printBranch(nextPrefix, isLastSection);
                std::cout << KEYWORD << "Components (" << VALUE << arch->components.size() << KEYWORD << ")" << RESET << "\n";
                std::string compPrefix = nextPrefix + (isLastSection ? TREE_SPACE : TREE_VERT);

                for (size_t i = 0; i < arch->components.size(); ++i)
                {
                    bool lastComponent = (i == arch->components.size() - 1);
                    printComponentDeclaration(arch->components[i].get(), compPrefix, lastComponent);
                }
            }

            // Instantiations
            if (hasInstantiations)
            {
                bool isLastSection = !hasAssignments && !hasProcesses;
                printBranch(nextPrefix, isLastSection);
                std::cout << KEYWORD << "Instantiations (" << VALUE << arch->instantiations.size() << KEYWORD << ")" << RESET << "\n";
                std::string instPrefix = nextPrefix + (isLastSection ? TREE_SPACE : TREE_VERT);

                for (size_t i = 0; i < arch->instantiations.size(); ++i)
                {
                    bool lastInst = (i == arch->instantiations.size() - 1);
                    printComponentInstantiation(arch->instantiations[i].get(), instPrefix, lastInst);
                }
            }

            // Assignments
            if (hasAssignments)
            {
                bool isLastSection = !hasProcesses;
                printBranch(nextPrefix, isLastSection);
                std::cout << KEYWORD << "Assignments (" << VALUE << arch->assignments.size() << KEYWORD << ")" << RESET << "\n";
                std::string assignPrefix = nextPrefix + (isLastSection ? TREE_SPACE : TREE_VERT);

                for (size_t i = 0; i < arch->assignments.size(); ++i)
                {
                    bool lastAssignment = (i == arch->assignments.size() - 1);
                    printSignalAssignment(arch->assignments[i].get(), assignPrefix, lastAssignment);
                }
            }

            // Processes
            if (hasProcesses)
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Processes (" << VALUE << arch->processes.size() << KEYWORD << ")" << RESET << "\n";
                std::string procPrefix = nextPrefix + TREE_SPACE;

                for (size_t i = 0; i < arch->processes.size(); ++i)
                {
                    bool lastProc = (i == arch->processes.size() - 1);
                    printProcessStatement(arch->processes[i].get(), procPrefix, lastProc);
                }
            }
        }

        /// Print a node with tree characters and proper indentation.
        void printNode(const ASTNode* node, const std::string& prefix, bool isLast)
        {
            if (!node)
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<nullptr>" << RESET << "\n";
                return;
            }

            if (auto entity = dynamic_cast<const EntityDeclaration*>(node))
                printEntityDeclaration(entity, prefix, isLast);
            else if (auto arch = dynamic_cast<const ArchitectureDeclaration*>(node))
                printArchitectureDeclaration(arch, prefix, isLast);
            else if (auto sig = dynamic_cast<const SignalDeclaration*>(node))
                printSignalDeclaration(sig, prefix, isLast);
            else if (auto port = dynamic_cast<const PortDeclaration*>(node))
                printPortDeclaration(port, prefix, isLast);
            else if (auto comp = dynamic_cast<const ComponentDeclaration*>(node))
                printComponentDeclaration(comp, prefix, isLast);
            else if (auto inst = dynamic_cast<const ComponentInstantiation*>(node))
                printComponentInstantiation(inst, prefix, isLast);
            else if (auto proc = dynamic_cast<const ProcessStatement*>(node))
                printProcessStatement(proc, prefix, isLast);
            else if (auto assign = dynamic_cast<const SignalAssignment*>(node))
                printSignalAssignment(assign, prefix, isLast);
            else
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<Unknown Node>" << RESET << "\n";
            }
        }

        // Templated expression helpers to eliminate repetition across ReturnType variants
        template <ReturnType R>
        bool tryPrintBinaryOp(const ASTNode* expr, const std::string& prefix, bool isLast, const char* typeStr)
        {
            if (auto binOp = dynamic_cast<const BinaryOpExpr<R>*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "BinaryOp" << TYPE << "[" << typeStr << "]: " << VALUE << binOp->op << RESET << "\n";

                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printBranch(nextPrefix, false);
                std::cout << KEYWORD << "left:" << RESET << "\n";
                printExpression(binOp->left.get(), nextPrefix + TREE_VERT, false);

                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "right:" << RESET << "\n";
                printExpression(binOp->right.get(), nextPrefix + TREE_SPACE, true);
                return true;
            }
            return false;
        }

        template <ReturnType R>
        bool tryPrintUnaryOp(const ASTNode* expr, const std::string& prefix, bool isLast, const char* typeStr)
        {
            if (auto unOp = dynamic_cast<const UnaryOpExpr<R>*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "UnaryOp" << TYPE << "[" << typeStr << "]: " << VALUE << unOp->op << RESET << "\n";

                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printExpression(unOp->operand.get(), nextPrefix, true);
                return true;
            }
            return false;
        }

        template <ReturnType R>
        bool tryPrintFunctionCall(const ASTNode* expr, const std::string& prefix, bool isLast, const char* typeStr)
        {
            if (auto fnCall = dynamic_cast<const FunctionCallExpr<R>*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "FunctionCall" << TYPE << "[" << typeStr << "]: " << IDENT << fnCall->functionName
                    << RESET << " (" << VALUE << fnCall->arguments.size() << RESET << " args)\n";

                if (!fnCall->arguments.empty())
                {
                    std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                    for (size_t i = 0; i < fnCall->arguments.size(); ++i)
                    {
                        bool lastArg = (i == fnCall->arguments.size() - 1);
                        printExpression(fnCall->arguments[i].get(), nextPrefix, lastArg);
                    }
                }
                return true;
            }
            return false;
        }

        void printExpression(const ASTNode* expr, const std::string& prefix, bool isLast)
        {
            if (!expr)
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<null expression>" << RESET << "\n";
                return;
            }

            // Try each concrete expression type
            if (auto sigRef = dynamic_cast<const SignalReference*>(expr))
            {
                printSignalReference(sigRef, prefix, isLast);
            }
            else if (auto intLit = dynamic_cast<const IntegerLiteralExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "IntegerLiteral: " << VALUE << intLit->value << RESET << "\n";
            }
            else if (auto logicLit = dynamic_cast<const LogicLiteralExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "LogicLiteral: " << RESET << "value=" << VALUE << "0x" << std::hex << logicLit->value
                    << RESET << " mask=" << VALUE << "0x" << logicLit->unknownMask << std::dec
                    << RESET << " width=" << VALUE << static_cast<int>(logicLit->width) << RESET << "\n";
            }
            else if (auto attr = dynamic_cast<const AttributeExpr*>(expr))
            {
                std::string attrName;
                switch (attr->kind)
                {
                    case AttributeKind::Left: attrName = "left"; break;
                    case AttributeKind::Right: attrName = "right"; break;
                    case AttributeKind::Low: attrName = "low"; break;
                    case AttributeKind::High: attrName = "high"; break;
                    case AttributeKind::Length: attrName = "length"; break;
                }
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "Attribute: " << IDENT << attrName << RESET << "\n";

                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "target:" << RESET << "\n";
                printSignalReference(attr->target.get(), nextPrefix + TREE_SPACE, true);
            }
            else if (auto whenExpr = dynamic_cast<const WhenElseExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "WhenElse (" << VALUE << whenExpr->branches.size() << KEYWORD << " branches)" << RESET << "\n";

                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

                for (size_t i = 0; i < whenExpr->branches.size(); ++i)
                {
                    const auto& branch = whenExpr->branches[i];
                    bool hasDefault = (whenExpr->defaultValue != nullptr);
                    bool isLastBranch = (i == whenExpr->branches.size() - 1) && !hasDefault;

                    printBranch(nextPrefix, isLastBranch);
                    std::cout << KEYWORD << "when #" << (i + 1) << RESET << "\n";
                    std::string branchPrefix = nextPrefix + (isLastBranch ? TREE_SPACE : TREE_VERT);

                    printBranch(branchPrefix, false);
                    std::cout << KEYWORD << "condition:" << RESET << "\n";
                    printExpression(branch.condition.get(), branchPrefix + TREE_VERT, false);

                    printBranch(branchPrefix, true);
                    std::cout << KEYWORD << "value:" << RESET << "\n";
                    printExpression(branch.value.get(), branchPrefix + TREE_SPACE, true);
                }

                if (whenExpr->defaultValue)
                {
                    printBranch(nextPrefix, true);
                    std::cout << KEYWORD << "else" << RESET << "\n";
                    printExpression(whenExpr->defaultValue.get(), nextPrefix + TREE_SPACE, true);
                }
            }
            else if (tryPrintBinaryOp<ReturnType::LOGIC>(expr, prefix, isLast, "LOGIC")) { }
            else if (tryPrintBinaryOp<ReturnType::INTEGER>(expr, prefix, isLast, "INTEGER")) { }
            else if (tryPrintBinaryOp<ReturnType::BOOLEAN>(expr, prefix, isLast, "BOOLEAN")) { }
            else if (tryPrintUnaryOp<ReturnType::LOGIC>(expr, prefix, isLast, "LOGIC")) { }
            else if (tryPrintUnaryOp<ReturnType::INTEGER>(expr, prefix, isLast, "INTEGER")) { }
            else if (tryPrintUnaryOp<ReturnType::BOOLEAN>(expr, prefix, isLast, "BOOLEAN")) { }
            else if (tryPrintFunctionCall<ReturnType::LOGIC>(expr, prefix, isLast, "LOGIC")) { }
            else if (tryPrintFunctionCall<ReturnType::INTEGER>(expr, prefix, isLast, "INTEGER")) { }
            else
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<Unknown Expression Type>" << RESET << "\n";
            }
        }
    } // anonymous namespace

    void ASTRoot::print() const
    {
        std::cout << "\n" << DIM << std::string(60, '=') << "\n";
        std::cout << "VHDL AST Tree" << "\n";
        std::cout << std::string(60, '=') << RESET << "\n\n";

        if (children.empty())
        {
            std::cout << DIM << "  <empty AST>" << RESET << "\n\n";
            return;
        }

        std::cout << KEYWORD << "ROOT" << RESET << "\n";
        for (size_t i = 0; i < children.size(); ++i)
        {
            bool isLast = (i == children.size() - 1);
            printNode(children[i].get(), "", isLast);
        }

        std::cout << "\n" << DIM << std::string(60, '=') << RESET << "\n\n";
    }

} // namespace Pulse::Parser