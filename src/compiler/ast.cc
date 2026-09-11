#include "ast.h"
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
        constexpr const char* DIM     = "\033[90m";   // Dark gray for tree lines & delimiters
        constexpr const char* KEYWORD = "\033[1;36m"; // Bold cyan for node types and labels
        constexpr const char* IDENT   = "\033[32m";   // Green for identifiers and names
        constexpr const char* VALUE   = "\033[33m";   // Yellow for values, counts, and numbers
        constexpr const char* TYPE    = "\033[35m";   // Magenta for type names & directions

        constexpr const char* TREE_TEE   = "\033[90m+-- \033[0m";
        constexpr const char* TREE_LAST  = "\033[90m+-- \033[0m";
        constexpr const char* TREE_VERT  = "\033[90m|   \033[0m";
        constexpr const char* TREE_SPACE = "    ";

        inline void printBranch(const std::string& prefix, bool isLast)
        {
            std::cout << prefix << (isLast ? TREE_LAST : TREE_TEE);
        }

        // Forward declarations
        void printNode(const ASTNode* node, const std::string& prefix, bool isLast);
        void printStatement(const Statement* stmt, const std::string& prefix, bool isLast);
        void printExpression(const Expression* expr, const std::string& prefix, bool isLast);
        void printSymbolExpr(const SymbolExpr* ref, const std::string& prefix, bool isLast);

        void printTypeSpec(const TypeSpec& ts, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "TYPE: " << TYPE << ts.typeName << RESET << "\n";
            if (!ts.args.empty())
            {
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "arguments (" << VALUE << ts.args.size() << KEYWORD << "):" << RESET << "\n";
                std::string argPrefix = nextPrefix + TREE_SPACE;
                for (size_t i = 0; i < ts.args.size(); ++i)
                {
                    printExpression(ts.args[i].get(), argPrefix, i == ts.args.size() - 1);
                }
            }
        }

        void printPortDeclaration(const PortDeclaration& port, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "PORT: " << IDENT << port.portName << " "
                << TYPE << "[" << (port.isInput ? (port.isOutput ? "INOUT" : "IN") : "OUT") << "]" << RESET << "\n";
            printTypeSpec(port.typeSpec, prefix + (isLast ? TREE_SPACE : TREE_VERT), true);
        }

        void printSignalDeclaration(const SignalDeclaration& signal, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "SIGNAL: " << IDENT << signal.name << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            bool hasInit = (signal.initialValue != nullptr);

            printTypeSpec(signal.typeSpec, nextPrefix, !hasInit);

            if (hasInit)
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "init:" << RESET << "\n";
                printExpression(signal.initialValue.get(), nextPrefix + TREE_SPACE, true);
            }
        }

        void printEntityDeclaration(const EntityDeclaration* entity, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "ENTITY: " << IDENT << entity->name << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            if (!entity->ports.empty())
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Ports (" << VALUE << entity->ports.size() << KEYWORD << ")" << RESET << "\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;
                for (size_t i = 0; i < entity->ports.size(); ++i)
                {
                    bool lastPort = (i == entity->ports.size() - 1);
                    printPortDeclaration(entity->ports[i], portPrefix, lastPort);
                }
            }
        }

        void printComponentDeclaration(const ComponentDeclaration& comp, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "COMPONENT: " << IDENT << comp.name << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            if (!comp.ports.empty())
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Ports (" << VALUE << comp.ports.size() << KEYWORD << ")" << RESET << "\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;
                for (size_t i = 0; i < comp.ports.size(); ++i)
                {
                    bool lastPort = (i == comp.ports.size() - 1);
                    printPortDeclaration(comp.ports[i], portPrefix, lastPort);
                }
            }
        }

        void printSymbolExpr(const SymbolExpr* sym, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            if (!sym)
            {
                std::cout << DIM << "<null SymbolExpr>" << RESET << "\n";
                return;
            }
            std::cout << KEYWORD << "SYMBOL: " << IDENT << sym->name << RESET << "\n";
        }

        void printComponentInstantiation(const ComponentInstantiation* inst, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "INSTANTIATION: " << IDENT << inst->instanceName
                << KEYWORD << " of entity " << IDENT << inst->componentName << RESET << "\n";

            if (!inst->portMap.empty())
            {
                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Port Maps (" << VALUE << inst->portMap.size() << KEYWORD << ")" << RESET << "\n";
                std::string portPrefix = nextPrefix + TREE_SPACE;

                for (size_t i = 0; i < inst->portMap.size(); ++i)
                {
                    bool lastMap = (i == inst->portMap.size() - 1);
                    const auto& [formalName, actualRef] = inst->portMap[i];
                    printBranch(portPrefix, lastMap);
                    std::cout << KEYWORD << "port: " << IDENT << formalName << RESET << "\n";
                    printSymbolExpr(&actualRef, portPrefix + (lastMap ? TREE_SPACE : TREE_VERT), true);
                }
            }
        }

        void printSignalAssignment(const SignalAssignment* assign, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "ASSIGNMENT" << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            bool hasValue = (assign->value != nullptr);

            printBranch(nextPrefix, !hasValue);
            std::cout << KEYWORD << "target:" << RESET << "\n";
            printExpression(assign->target.get(), nextPrefix + (hasValue ? TREE_VERT : TREE_SPACE), true);

            if (hasValue)
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "value:" << RESET << "\n";
                printExpression(assign->value.get(), nextPrefix + TREE_SPACE, true);
            }
        }

        void printWithClause(const WithClause* withClause, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "WITH SELECT (" 
            << withClause->choices.size() 
            << (withClause->defaultValue ? " branches + default" : " branches") 
            << ")" << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);

            printBranch(nextPrefix, false);
            std::cout << KEYWORD << "selector:" << RESET << "\n";
            printExpression(withClause->selector.get(), nextPrefix + TREE_VERT, true);

            for (size_t i = 0; i < withClause->choices.size(); ++i)
            {
                bool lastChoice = (i == withClause->choices.size() - 1) && !withClause->defaultValue;
                printBranch(nextPrefix, lastChoice);
                std::cout << KEYWORD << "choice #" << (i + 1) << ":" << RESET << "\n";
                std::string choicePrefix = nextPrefix + (lastChoice ? TREE_SPACE : TREE_VERT);

                printBranch(choicePrefix, false);
                std::cout << KEYWORD << "when:" << RESET << "\n";
                printExpression(withClause->choices[i].second.get(), choicePrefix + TREE_VERT, true);

                printBranch(choicePrefix, true);
                std::cout << KEYWORD << "value:" << RESET << "\n";
                printExpression(withClause->choices[i].first.get(), choicePrefix + TREE_SPACE, true);
            }

            if (withClause->defaultValue)
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "others (default):" << RESET << "\n";
                printExpression(withClause->defaultValue.get(), nextPrefix + TREE_SPACE, true);
            }
        }

        void printWaitForStatement(const WaitForStatement* wait, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "WAIT FOR: " << VALUE << wait->durationFs << RESET << " fs\n";
        }

        void printWaitForeverStatement(const WaitForeverStatement*, const std::string& prefix, bool isLast)
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
                std::cout << KEYWORD << (i == 0 ? "if:" : "elsif:") << RESET << "\n";
                std::string branchPrefix = nextPrefix + (isLastBranch ? TREE_SPACE : TREE_VERT);

                printBranch(branchPrefix, branch.body.empty());
                std::cout << KEYWORD << "condition:" << RESET << "\n";
                printExpression(branch.condition.get(), branchPrefix + (branch.body.empty() ? TREE_SPACE : TREE_VERT), true);

                if (!branch.body.empty())
                {
                    printBranch(branchPrefix, true);
                    std::cout << KEYWORD << "body (" << VALUE << branch.body.size() << KEYWORD << "):" << RESET << "\n";
                    std::string bodyPrefix = branchPrefix + TREE_SPACE;
                    for (size_t j = 0; j < branch.body.size(); ++j)
                        printStatement(branch.body[j].get(), bodyPrefix, j == branch.body.size() - 1);
                }
            }

            if (!ifStmt->elseBody.empty())
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "else:" << RESET << "\n";
                std::string elsePrefix = nextPrefix + TREE_SPACE;
                for (size_t j = 0; j < ifStmt->elseBody.size(); ++j)
                    printStatement(ifStmt->elseBody[j].get(), elsePrefix, j == ifStmt->elseBody.size() - 1);
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

            if (!proc->sensitivityList.empty())
            {
                bool hasBody = !proc->body.empty();
                printBranch(nextPrefix, !hasBody);
                std::cout << KEYWORD << "sensitivity (" << VALUE << proc->sensitivityList.size() << KEYWORD << "):" << RESET;
                for (const auto& sig : proc->sensitivityList)
                    std::cout << " " << IDENT << sig << RESET;
                std::cout << "\n";
            }

            if (!proc->body.empty())
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "body (" << VALUE << proc->body.size() << KEYWORD << "):" << RESET << "\n";
                std::string bodyPrefix = nextPrefix + TREE_SPACE;
                for (size_t i = 0; i < proc->body.size(); ++i)
                    printStatement(proc->body[i].get(), bodyPrefix, i == proc->body.size() - 1);
            }
        }

        void printStatement(const Statement* stmt, const std::string& prefix, bool isLast)
        {
            if (!stmt)
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<null statement>" << RESET << "\n";
                return;
            }

            if (auto assign = dynamic_cast<const SignalAssignment*>(stmt))
                printSignalAssignment(assign, prefix, isLast);
            else if (auto withClause = dynamic_cast<const WithClause*>(stmt))
                printWithClause(withClause, prefix, isLast);
            else if (auto inst = dynamic_cast<const ComponentInstantiation*>(stmt))
                printComponentInstantiation(inst, prefix, isLast);
            else if (auto proc = dynamic_cast<const ProcessStatement*>(stmt))
                printProcessStatement(proc, prefix, isLast);
            else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt))
                printIfStatement(ifStmt, prefix, isLast);
            else if (auto wait = dynamic_cast<const WaitForStatement*>(stmt))
                printWaitForStatement(wait, prefix, isLast);
            else if (auto waitForever = dynamic_cast<const WaitForeverStatement*>(stmt))
                printWaitForeverStatement(waitForever, prefix, isLast);
            else
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<Unknown Statement>" << RESET << "\n";
            }
        }

        void printArchitectureDeclaration(const ArchitectureDeclaration* arch, const std::string& prefix, bool isLast)
        {
            printBranch(prefix, isLast);
            std::cout << KEYWORD << "ARCHITECTURE: " << IDENT << arch->name
                << KEYWORD << " OF " << IDENT << arch->entityName << RESET << "\n";

            std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
            bool hasSignals = !arch->signals.empty();
            bool hasComponents = !arch->components.empty();
            bool hasBody = !arch->body.empty();

            if (hasSignals)
            {
                bool isLastSection = !hasComponents && !hasBody;
                printBranch(nextPrefix, isLastSection);
                std::cout << KEYWORD << "Signals (" << VALUE << arch->signals.size() << KEYWORD << ")" << RESET << "\n";
                std::string sigPrefix = nextPrefix + (isLastSection ? TREE_SPACE : TREE_VERT);
                for (size_t i = 0; i < arch->signals.size(); ++i)
                    printSignalDeclaration(arch->signals[i], sigPrefix, i == arch->signals.size() - 1);
            }

            if (hasComponents)
            {
                bool isLastSection = !hasBody;
                printBranch(nextPrefix, isLastSection);
                std::cout << KEYWORD << "Components (" << VALUE << arch->components.size() << KEYWORD << ")" << RESET << "\n";
                std::string compPrefix = nextPrefix + (isLastSection ? TREE_SPACE : TREE_VERT);
                for (size_t i = 0; i < arch->components.size(); ++i)
                    printComponentDeclaration(arch->components[i], compPrefix, i == arch->components.size() - 1);
            }

            if (hasBody)
            {
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "Body (" << VALUE << arch->body.size() << KEYWORD << ")" << RESET << "\n";
                std::string bodyPrefix = nextPrefix + TREE_SPACE;
                for (size_t i = 0; i < arch->body.size(); ++i)
                    printStatement(arch->body[i].get(), bodyPrefix, i == arch->body.size() - 1);
            }
        }

        void printExpression(const Expression* expr, const std::string& prefix, bool isLast)
        {
            if (!expr)
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<null expression>" << RESET << "\n";
                return;
            }

            if (auto symRef = dynamic_cast<const SymbolExpr*>(expr))
            {
                printSymbolExpr(symRef, prefix, isLast);
            }
            else if (auto intLit = dynamic_cast<const IntegerLiteralExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "INTEGER LITERAL: " << VALUE << intLit->value << RESET << "\n";
            }
            else if (auto boolLit = dynamic_cast<const BooleanLiteral*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "BOOLEAN LITERAL: " << VALUE << (boolLit->value ? "true" : "false") << RESET << "\n";
            }
            else if (auto logicLit = dynamic_cast<const LogicLiteralExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << (logicLit->isSigned ? "LOGIC LITERAL (signed):" : "LOGIC LITERAL (unsigned): ")
                    << RESET << " value=" << VALUE << "0x" << std::hex << logicLit->value
                    << RESET << " mask=" << VALUE << "0x" << logicLit->mask << std::dec
                    << RESET << " width=" << VALUE << static_cast<int>(logicLit->width)
                    << RESET << "\n";
            }
            else if (auto attr = dynamic_cast<const AttributeExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "ATTRIBUTE ACCESS: " << IDENT << attr->attributeName << RESET << "\n";
                if (attr->target)
                {
                    std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                    printBranch(nextPrefix, true);
                    std::cout << KEYWORD << "target:" << RESET << "\n";
                    printSymbolExpr(attr->target.get(), nextPrefix + TREE_SPACE, true);
                }
            }
            else if (auto whenExpr = dynamic_cast<const WhenElseExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "WHEN ELSE" << RESET << "\n";

                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                bool hasFalseValue = (whenExpr->falseValue != nullptr);

                // 1. Condition
                printBranch(nextPrefix, false);
                std::cout << KEYWORD << "condition:" << RESET << "\n";
                printExpression(whenExpr->condition.get(), nextPrefix + TREE_VERT, true);

                // 2. True Value (what is assigned if condition is true)
                printBranch(nextPrefix, !hasFalseValue);
                std::cout << KEYWORD << "true value:" << RESET << "\n";
                printExpression(whenExpr->trueValue.get(), nextPrefix + (hasFalseValue ? TREE_VERT : TREE_SPACE), true);

                // 3. False Value (the "else" clause, which may be another WhenElseExpr)
                if (hasFalseValue)
                {
                    printBranch(nextPrefix, true);
                    std::cout << KEYWORD << "false value:" << RESET << "\n";
                    printExpression(whenExpr->falseValue.get(), nextPrefix + TREE_SPACE, true);
                }
            }
            else if (auto binOp = dynamic_cast<const BinaryOpExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "BINARY OP: " << KEYWORD << binOp->op << RESET << "\n";

                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printBranch(nextPrefix, false);
                std::cout << KEYWORD << "left:" << RESET << "\n";
                printExpression(binOp->left.get(), nextPrefix + TREE_VERT, false);

                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "right:" << RESET << "\n";
                printExpression(binOp->right.get(), nextPrefix + TREE_SPACE, true);
            }
            else if (auto unOp = dynamic_cast<const UnaryOpExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "UNARY OP: " << KEYWORD << unOp->op << RESET << "\n";

                std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                printBranch(nextPrefix, true);
                std::cout << KEYWORD << "operand:" << RESET << "\n";
                printExpression(unOp->operand.get(), nextPrefix + TREE_SPACE, true);
            }
            else if (auto fnCall = dynamic_cast<const FunctionCallExpr*>(expr))
            {
                printBranch(prefix, isLast);
                std::cout << KEYWORD << "FUNCTION CALL: " << IDENT << fnCall->functionName << RESET << "\n";

                if (!fnCall->arguments.empty())
                {
                    std::string nextPrefix = prefix + (isLast ? TREE_SPACE : TREE_VERT);
                    printBranch(nextPrefix, true);
                    std::cout << KEYWORD << "arguments (" << VALUE << fnCall->arguments.size() << KEYWORD << "):" << RESET << "\n";
                    std::string argPrefix = nextPrefix + TREE_SPACE;
                    for (size_t i = 0; i < fnCall->arguments.size(); ++i)
                        printExpression(fnCall->arguments[i].get(), argPrefix, i == fnCall->arguments.size() - 1);
                }
            }
            else
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<Unknown Expression>" << RESET << "\n";
            }
        }

        void printNode(const ASTNode* node, const std::string& prefix, bool isLast)
        {
            if (!node)
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<nullptr>" << RESET << "\n";
                return;
            }

            if (auto typeSpec = dynamic_cast<const TypeSpec*>(node))
                printTypeSpec(*typeSpec, prefix, isLast);
            else if (auto entity = dynamic_cast<const EntityDeclaration*>(node))
                printEntityDeclaration(entity, prefix, isLast);
            else if (auto arch = dynamic_cast<const ArchitectureDeclaration*>(node))
                printArchitectureDeclaration(arch, prefix, isLast);
            else if (auto port = dynamic_cast<const PortDeclaration*>(node))
                printPortDeclaration(*port, prefix, isLast);
            else if (auto sig = dynamic_cast<const SignalDeclaration*>(node))
                printSignalDeclaration(*sig, prefix, isLast);
            else if (auto comp = dynamic_cast<const ComponentDeclaration*>(node))
                printComponentDeclaration(*comp, prefix, isLast);
            else if (auto stmt = dynamic_cast<const Statement*>(node))
                printStatement(stmt, prefix, isLast);
            else if (auto expr = dynamic_cast<const Expression*>(node))
                printExpression(expr, prefix, isLast);
            else
            {
                printBranch(prefix, isLast);
                std::cout << DIM << "<Unknown Node>" << RESET << "\n";
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