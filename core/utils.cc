#ifndef PULSE_VHDL_ASTPRINTER_H
#define PULSE_VHDL_ASTPRINTER_H

#include "ASTBuilder.h"
#include <iostream>
#include <string>
#include <memory>
#include <iomanip>

namespace Pulse::Parser::VHDL
{
    /// Utility class for pretty-printing the full AST tree structure for debugging.
    class ASTPrinter
    {
    private:
        // ASCII-only characters for maximum compatibility
        static constexpr const char* TREE_TEE = "+-- ";
        static constexpr const char* TREE_LAST = "+-- ";
        static constexpr const char* TREE_VERT = "|   ";
        static constexpr const char* TREE_SPACE = "    ";

        /// Print a node with tree characters and proper indentation.
        static void printNode(const ASTNode* node, const std::string& prefix, bool isLast)
        {
            if (!node)
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE) << "<nullptr>\n";
                return;
            }

            if (auto entity = dynamic_cast<const EntityDeclaration*>(node))
            {
                printEntityDeclaration(entity, prefix, isLast);
            }
            else if (auto arch = dynamic_cast<const ArchitectureDeclaration*>(node))
            {
                printArchitectureDeclaration(arch, prefix, isLast);
            }
            else if (auto sig = dynamic_cast<const SignalDeclaration*>(node))
            {
                printSignalDeclaration(sig, prefix, isLast);
            }
            else if (auto port = dynamic_cast<const PortDeclaration*>(node))
            {
                printPortDeclaration(port, prefix, isLast);
            }
            else if (auto comp = dynamic_cast<const ComponentDeclaration*>(node))
            {
                printComponentDeclaration(comp, prefix, isLast);
            }
            else if (auto inst = dynamic_cast<const ComponentInstantiation*>(node))
            {
                printComponentInstantiation(inst, prefix, isLast);
            }
            else if (auto assign = dynamic_cast<const SignalAssignment*>(node))
            {
                printSignalAssignment(assign, prefix, isLast);
            }
            else
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE) << "<Unknown Node>\n";
            }
        }

        static void printPortDeclaration(const PortDeclaration* port,
                                        const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                      << "PORT: " << port->portName 
                      << " [" << (port->isInput ? "IN" : "OUT") << "] "
                      << "width=" << static_cast<int>(port->width) << "\n";
        }

        static void printSignalDeclaration(const SignalDeclaration* signal,
                                          const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                      << "SIGNAL: " << signal->signalName 
                      << " width=" << static_cast<int>(signal->width);
            
            if (signal->initialValue != 0)
                std::cout << " init=0x" << std::hex << signal->initialValue << std::dec;
            std::cout << "\n";
        }

        static void printEntityDeclaration(const EntityDeclaration* entity, 
                                          const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE) 
                      << "ENTITY: " << entity->entityName << "\n";
            
            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            bool hasPorts = !entity->ports.empty();

            if (hasPorts)
            {
                std::cout << nextPrefix << TREE_LAST << "Ports (" << entity->ports.size() << ")\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;
                
                for (size_t i = 0; i < entity->ports.size(); ++i)
                {
                    bool lastPort = (i == entity->ports.size() - 1);
                    printPortDeclaration(entity->ports[i].get(), portPrefix, lastPort);
                }
            }
        }

        static void printComponentDeclaration(const ComponentDeclaration* comp,
                                             const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                      << "COMPONENT: " << comp->componentName << "\n";
            
            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            bool hasPorts = !comp->ports.empty();

            if (hasPorts)
            {
                std::cout << nextPrefix << TREE_LAST << "Ports (" << comp->ports.size() << ")\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;

                for (size_t i = 0; i < comp->ports.size(); ++i)
                {
                    bool lastPort = (i == comp->ports.size() - 1);
                    printPortDeclaration(comp->ports[i].get(), portPrefix, lastPort);
                }
            }
        }

        static std::string returnTypeToString(ReturnType rt)
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

        static void printSignalReference(const SignalReference* ref, const std::string& prefix, bool isLast)
        {
            if (!ref)
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE) << "<null SignalReference>\n";
                return;
            }

            std::string indexDesc;
            if (!ref->low && !ref->high)
                indexDesc = " (full signal)";
            else if (ref->low && !ref->high)
                indexDesc = " (single bit)";
            else
                indexDesc = " (range)";

            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                      << "SignalReference: " << ref->signalName << indexDesc << "\n";

            if (ref->low || ref->high)
            {
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                
                if (ref->low)
                {
                    bool lastBound = !ref->high;
                    std::cout << nextPrefix << (lastBound ? TREE_LAST : TREE_TEE) << "low:\n";
                    printExpression(ref->low.get(), nextPrefix + (lastBound ? TREE_SPACE : TREE_VERT), true);
                }
                
                if (ref->high)
                {
                    std::cout << nextPrefix << TREE_LAST << "high:\n";
                    printExpression(ref->high.get(), nextPrefix + TREE_SPACE, true);
                }
            }
        }

        static void printComponentInstantiation(const ComponentInstantiation* inst,
                                                const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                      << "INSTANTIATION: " << inst->instanceName 
                      << " OF " << inst->componentName << "\n";

            if (!inst->portMaps.empty())
            {
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                std::cout << nextPrefix << TREE_LAST << "Port Maps (" << inst->portMaps.size() << ")\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;
                
                size_t idx = 0;
                for (const auto& [formalName, actualRef] : inst->portMaps)
                {
                    bool lastMap = (++idx == inst->portMaps.size());
                    std::cout << portPrefix << (lastMap ? TREE_LAST : TREE_TEE)
                              << "formal: " << formalName << "\n";
                    printSignalReference(actualRef.get(), portPrefix + (lastMap ? TREE_SPACE : TREE_VERT), true);
                }
            }
        }

        static void printSignalAssignment(const SignalAssignment* assign,
                                         const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE) << "ASSIGNMENT\n";
            
            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            
            // Print Target SignalReference
            bool hasValue = (assign->value != nullptr);
            std::cout << nextPrefix << (hasValue ? TREE_TEE : TREE_LAST) << "target:\n";
            printSignalReference(assign->target.get(), nextPrefix + (hasValue ? TREE_VERT : TREE_SPACE), true);

            // Print Value (could be WhenElseExpr or simple expression)
            if (hasValue)
            {
                std::cout << nextPrefix << TREE_LAST << "value:\n";
                printExpression(assign->value.get(), nextPrefix + TREE_SPACE, true);
            }
        }

        static void printArchitectureDeclaration(const ArchitectureDeclaration* arch,
                                                 const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                      << "ARCHITECTURE: " << arch->architectureName 
                      << " OF " << arch->entityName << "\n";
            
            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            bool hasSignals = !arch->signals.empty();
            bool hasComponents = !arch->components.empty();
            bool hasInstantiations = !arch->instantiations.empty();
            bool hasAssignments = !arch->assignments.empty();

            // Signals
            if (hasSignals)
            {
                bool isLastSection = !hasComponents && !hasInstantiations && !hasAssignments;
                std::cout << nextPrefix << (isLastSection ? TREE_LAST : TREE_TEE) 
                          << "Signals (" << arch->signals.size() << ")\n";
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
                bool isLastSection = !hasInstantiations && !hasAssignments;
                std::cout << nextPrefix << (isLastSection ? TREE_LAST : TREE_TEE) 
                          << "Components (" << arch->components.size() << ")\n";
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
                bool isLastSection = !hasAssignments;
                std::cout << nextPrefix << (isLastSection ? TREE_LAST : TREE_TEE) 
                          << "Instantiations (" << arch->instantiations.size() << ")\n";
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
                std::cout << nextPrefix << TREE_LAST << "Assignments (" 
                          << arch->assignments.size() << ")\n";
                std::string assignPrefix = nextPrefix + TREE_SPACE;
                
                for (size_t i = 0; i < arch->assignments.size(); ++i)
                {
                    bool lastAssignment = (i == arch->assignments.size() - 1);
                    printSignalAssignment(arch->assignments[i].get(), assignPrefix, lastAssignment);
                }
            }
        }

        static void printExpression(const ASTNode* expr, const std::string& prefix, bool isLast)
        {
            if (!expr)
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE) << "<null expression>\n";
                return;
            }

            // Try each concrete expression type
            if (auto sigRef = dynamic_cast<const SignalReference*>(expr))
            {
                printSignalReference(sigRef, prefix, isLast);
            }
            else if (auto intLit = dynamic_cast<const IntegerLiteralExpr*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "IntegerLiteral: " << intLit->value << "\n";
            }
            else if (auto logicLit = dynamic_cast<const LogicLiteralExpr*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "LogicLiteral: value=0x" << std::hex << logicLit->value
                          << " mask=0x" << logicLit->unknownMask << std::dec
                          << " width=" << static_cast<int>(logicLit->width) << "\n";
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
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "Attribute: " << attrName << "\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                std::cout << nextPrefix << TREE_LAST << "target:\n";
                printSignalReference(attr->target.get(), nextPrefix + TREE_SPACE, true);
            }
            else if (auto whenExpr = dynamic_cast<const WhenElseExpr*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "WhenElse (" << whenExpr->branches.size() << " branches)\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                
                for (size_t i = 0; i < whenExpr->branches.size(); ++i)
                {
                    const auto& branch = whenExpr->branches[i];
                    bool hasDefault = (whenExpr->defaultValue != nullptr);
                    bool isLastBranch = (i == whenExpr->branches.size() - 1) && !hasDefault;
                    
                    std::cout << nextPrefix << (isLastBranch ? TREE_LAST : TREE_TEE) << "when #" << (i+1) << "\n";
                    std::string branchPrefix = nextPrefix + (isLastBranch ? TREE_SPACE : TREE_VERT);
                    
                    std::cout << branchPrefix << TREE_TEE << "condition:\n";
                    printExpression(branch.condition.get(), branchPrefix + TREE_VERT, false);
                    
                    std::cout << branchPrefix << TREE_LAST << "value:\n";
                    printExpression(branch.value.get(), branchPrefix + TREE_SPACE, true);
                }
                
                if (whenExpr->defaultValue)
                {
                    std::cout << nextPrefix << TREE_LAST << "else\n";
                    printExpression(whenExpr->defaultValue.get(), nextPrefix + TREE_SPACE, true);
                }
            }
            else if (auto binOp = dynamic_cast<const BinaryOpExpr<ReturnType::LOGIC>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "BinaryOp[LOGIC]: " << binOp->op << "\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                std::cout << nextPrefix << TREE_TEE << "left:\n";
                printExpression(binOp->left.get(), nextPrefix + TREE_VERT, false);
                
                std::cout << nextPrefix << TREE_LAST << "right:\n";
                printExpression(binOp->right.get(), nextPrefix + TREE_SPACE, true);
            }
            else if (auto binOp = dynamic_cast<const BinaryOpExpr<ReturnType::INTEGER>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "BinaryOp[INTEGER]: " << binOp->op << "\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                std::cout << nextPrefix << TREE_TEE << "left:\n";
                printExpression(binOp->left.get(), nextPrefix + TREE_VERT, false);
                
                std::cout << nextPrefix << TREE_LAST << "right:\n";
                printExpression(binOp->right.get(), nextPrefix + TREE_SPACE, true);
            }
            else if (auto binOp = dynamic_cast<const BinaryOpExpr<ReturnType::BOOLEAN>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "BinaryOp[BOOLEAN]: " << binOp->op << "\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                std::cout << nextPrefix << TREE_TEE << "left:\n";
                printExpression(binOp->left.get(), nextPrefix + TREE_VERT, false);
                
                std::cout << nextPrefix << TREE_LAST << "right:\n";
                printExpression(binOp->right.get(), nextPrefix + TREE_SPACE, true);
            }
            else if (auto unOp = dynamic_cast<const UnaryOpExpr<ReturnType::LOGIC>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "UnaryOp[LOGIC]: " << unOp->op << "\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printExpression(unOp->operand.get(), nextPrefix, true);
            }
            else if (auto unOp = dynamic_cast<const UnaryOpExpr<ReturnType::INTEGER>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "UnaryOp[INTEGER]: " << unOp->op << "\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printExpression(unOp->operand.get(), nextPrefix, true);
            }
            else if (auto unOp = dynamic_cast<const UnaryOpExpr<ReturnType::BOOLEAN>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "UnaryOp[BOOLEAN]: " << unOp->op << "\n";
                
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printExpression(unOp->operand.get(), nextPrefix, true);
            }
            else if (auto fnCall = dynamic_cast<const FunctionCallExpr<ReturnType::LOGIC>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "FunctionCall[LOGIC]: " << fnCall->functionName 
                          << " (" << fnCall->arguments.size() << " args)\n";

                if (!fnCall->arguments.empty())
                {
                    std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                    for (size_t i = 0; i < fnCall->arguments.size(); ++i)
                    {
                        bool lastArg = (i == fnCall->arguments.size() - 1);
                        printExpression(fnCall->arguments[i].get(), nextPrefix, lastArg);
                    }
                }
            }
            else if (auto fnCall = dynamic_cast<const FunctionCallExpr<ReturnType::INTEGER>*>(expr))
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "FunctionCall[INTEGER]: " << fnCall->functionName 
                          << " (" << fnCall->arguments.size() << " args)\n";

                if (!fnCall->arguments.empty())
                {
                    std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                    for (size_t i = 0; i < fnCall->arguments.size(); ++i)
                    {
                        bool lastArg = (i == fnCall->arguments.size() - 1);
                        printExpression(fnCall->arguments[i].get(), nextPrefix, lastArg);
                    }
                }
            }
            else
            {
                std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE)
                          << "<Unknown Expression Type>\n";
            }
        }

    public:
        /// Print the entire AST tree rooted at the given RootNode.
        static void printTree(const RootNode& root)
        {
            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "VHDL AST Tree\n";
            std::cout << std::string(60, '=') << "\n\n";
            
            if (root.children.empty())
            {
                std::cout << "  <empty AST>\n\n";
                return;
            }

            std::cout << "ROOT\n";
            for (size_t i = 0; i < root.children.size(); ++i)
            {
                bool isLast = (i == root.children.size() - 1);
                printNode(root.children[i].get(), "", isLast);
            }
            
            std::cout << "\n" << std::string(60, '=') << "\n\n";
        }

        /// Print comprehensive statistics of the AST components.
        static void printStats(const RootNode& root)
        {
            size_t entityCount = 0;
            size_t archCount = 0;
            size_t portCount = 0;
            size_t signalCount = 0;
            size_t componentCount = 0;
            size_t instantiationCount = 0;
            size_t assignmentCount = 0;

            for (const auto& child : root.children)
            {
                if (auto entity = dynamic_cast<const EntityDeclaration*>(child.get()))
                {
                    entityCount++;
                    portCount += entity->ports.size();
                }
                else if (auto arch = dynamic_cast<const ArchitectureDeclaration*>(child.get()))
                {
                    archCount++;
                    signalCount += arch->signals.size();
                    componentCount += arch->components.size();
                    instantiationCount += arch->instantiations.size();
                    assignmentCount += arch->assignments.size();
                }
            }

            std::cout << "\nAST Statistics:\n";
            std::cout << "  Entities:       " << entityCount << "\n";
            std::cout << "  Architectures:  " << archCount << "\n";
            std::cout << "  Ports:          " << portCount << "\n";
            std::cout << "  Signals:        " << signalCount << "\n";
            std::cout << "  Components:     " << componentCount << "\n";
            std::cout << "  Instantiations: " << instantiationCount << "\n";
            std::cout << "  Assignments:    " << assignmentCount << "\n\n";
        }
    };

} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_ASTPRINTER_H