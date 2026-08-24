#include "semanticAnalyzer.h"
#include <cmath>

namespace Pulse::Parser::VHDL
{
    void SemanticAnalyzer::analyze(const ASTRoot& root)
    {
        // 1. First pass: Register all available entities in the global scope
        registerEntities(root);

        // 2. Second pass: Analyze all architectures against their bound entities
        for (const auto& child : root.children)
        {
            if (auto arch = dynamic_cast<const ArchitectureDeclaration*>(child.get()))
            {
                analyzeArchitecture(arch);
            }
        }
    }

    void SemanticAnalyzer::registerEntities(const ASTRoot& root)
    {
        for (const auto& child : root.children)
        {
            if (auto entity = dynamic_cast<const EntityDeclaration*>(child.get()))
            {
                if (m_globalEntities.find(entity->entityName) != m_globalEntities.end())
                {
                    throw std::runtime_error("Semantic Error: Duplicate entity declaration '" + entity->entityName + "'");
                }
                m_globalEntities[entity->entityName] = entity;
            }
        }
    }

    void SemanticAnalyzer::analyzeArchitecture(const ArchitectureDeclaration* arch)
    {
        m_currentScopeSymbols.clear();
        m_currentComponents.clear();
        m_processLabels.clear();

        // Ensure the architecture binds to a valid entity
        auto it = m_globalEntities.find(arch->entityName);
        if (it == m_globalEntities.end())
        {
            throw std::runtime_error("Semantic Error: Architecture '" + arch->architectureName + 
                                     "' references unknown entity '" + arch->entityName + "'");
        }
        
        const EntityDeclaration* boundEntity = it->second;

        // Load entity ports into the local scope
        for (const auto& port : boundEntity->ports)
        {
            m_currentScopeSymbols[port->portName] = { port->width, port->isInput };
        }

        // Load internal signals into the local scope
        for (const auto& sig : arch->signals)
        {
            if (m_currentScopeSymbols.find(sig->signalName) != m_currentScopeSymbols.end())
            {
                throw std::runtime_error("Semantic Error: Signal '" + sig->signalName + "' shadows an existing port.");
            }
            m_currentScopeSymbols[sig->signalName] = { sig->width, false };
        }

        // Load local component declarations
        for (const auto& comp : arch->components)
        {
            m_currentComponents[comp->componentName] = comp.get();
        }

        // Analyze concurrent assignments
        for (const auto& assignment : arch->assignments)
        {
            analyzeAssignment(assignment.get());
        }

        // Analyze component instantiations (Port Mapping)
        for (const auto& inst : arch->instantiations)
        {
            analyzeInstantiation(inst.get(), arch);
        }

        // Analyze processes
        for (const auto& proc : arch->processes)
        {
            analyzeProcess(proc.get());
        }
    }

    void SemanticAnalyzer::analyzeAssignment(const SignalAssignment* assignment)
    {
        Pulse::bitWidth_t targetWidth = getSignalReferenceWidth(assignment->target.get());
        Pulse::bitWidth_t valueWidth = evaluateWidth(assignment->value.get());

        // Validate width matching
        if (targetWidth != valueWidth)
        {
            throw std::runtime_error("Semantic Error: Width mismatch in assignment to '" + 
                                     assignment->target->signalName + "'. Target is " + 
                                     std::to_string(targetWidth) + " bits, but value is " + 
                                     std::to_string(valueWidth) + " bits.");
        }

        // Validate directional rules (cannot write to an input port)
        auto it = m_currentScopeSymbols.find(assignment->target->signalName);
        if (it != m_currentScopeSymbols.end() && it->second.isInput)
        {
            throw std::runtime_error("Semantic Error: Cannot assign to input port '" + 
                                     assignment->target->signalName + "'.");
        }
    }

    void SemanticAnalyzer::analyzeInstantiation(const ComponentInstantiation* inst, const ArchitectureDeclaration* arch)
    {
        // Resolve target component
        auto compIt = m_currentComponents.find(inst->componentName);
        if (compIt == m_currentComponents.end())
        {
            throw std::runtime_error("Semantic Error: Unrecognized component '" + 
                                     inst->componentName + "' in instance '" + inst->instanceName + "'.");
        }
        
        const ComponentDeclaration* comp = compIt->second;

        // Verify port mappings
        for (const auto& mappedPort : inst->portMaps)
        {
            bool found = false;
            for (const auto& formalPort : comp->ports)
            {
                if (formalPort->portName == mappedPort.first)
                {
                    found = true;
                    Pulse::bitWidth_t actualWidth = getSignalReferenceWidth(mappedPort.second.get());
                    
                    if (formalPort->width != actualWidth)
                    {
                        throw std::runtime_error("Semantic Error: Port map width mismatch for '" + 
                                                 mappedPort.first + "' in instance '" + inst->instanceName + "'.");
                    }
                    break;
                }
            }

            if (!found)
            {
                throw std::runtime_error("Semantic Error: Port '" + mappedPort.first + 
                                         "' does not exist on component '" + inst->componentName + "'.");
            }
        }
    }

    Pulse::bitWidth_t SemanticAnalyzer::getSignalReferenceWidth(const SignalReference* ref)
    {
        auto it = m_currentScopeSymbols.find(ref->signalName);
        if (it == m_currentScopeSymbols.end())
        {
            throw std::runtime_error("Semantic Error: Undeclared signal referenced '" + ref->signalName + "'");
        }

        // Full signal reference (no bounds provided)
        if (!ref->low && !ref->high) return it->second.width;
        
        // Single bit index access
        if (ref->low && !ref->high) return 1;

        // Range access (slice)
        if (ref->low && ref->high)
        {
            auto lowLit = dynamic_cast<IntegerLiteralExpr*>(ref->low.get());
            auto highLit = dynamic_cast<IntegerLiteralExpr*>(ref->high.get());
            
            if (lowLit && highLit)
            {
                return static_cast<Pulse::bitWidth_t>(std::abs(highLit->value - lowLit->value) + 1);
            }
            throw std::runtime_error("Semantic Error: Dynamic ranges are not supported for width calculation.");
        }
        return 1;
    }

    Pulse::bitWidth_t SemanticAnalyzer::evaluateWidth(const ASTNode* node)
    {
        if (auto lit = dynamic_cast<const LogicLiteralExpr*>(node)) return lit->width;
        if (auto ref = dynamic_cast<const SignalReference*>(node)) return getSignalReferenceWidth(ref);
        
        if (auto binOp = dynamic_cast<const BinaryOpExpr<ReturnType::LOGIC>*>(node))
        {
            Pulse::bitWidth_t leftW = evaluateWidth(binOp->left.get());
            Pulse::bitWidth_t rightW = evaluateWidth(binOp->right.get());
            
            if (binOp->op == "&") return leftW + rightW;
            
            if (leftW != rightW && binOp->op != "sll" && binOp->op != "srl")
            {
                throw std::runtime_error("Semantic Error: Operand width mismatch in binary operation '" + binOp->op + "'.");
            }
            return leftW;
        }

        if (auto unOp = dynamic_cast<const UnaryOpExpr<ReturnType::LOGIC>*>(node))
        {
            return evaluateWidth(unOp->operand.get());
        }

        if (auto whenElse = dynamic_cast<const WhenElseExpr*>(node))
        {
            Pulse::bitWidth_t defaultW = evaluateWidth(whenElse->defaultValue.get());
            for (const auto& branch : whenElse->branches)
            {
                if (evaluateWidth(branch.value.get()) != defaultW)
                {
                    throw std::runtime_error("Semantic Error: Width mismatch in WHEN/ELSE branches.");
                }
            }
            return defaultW;
        }
        
        throw std::runtime_error("Semantic Error: Unable to determine expression width.");
    }

    namespace
    {
        bool hasWaitStatements(const std::vector<std::unique_ptr<SequentialStatement>>& body)
        {
            for (const auto& stmt : body)
            {
                if (dynamic_cast<const WaitForStatement*>(stmt.get()))
                {
                    return true;
                }
                if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt.get()))
                {
                    for (const auto& branch : ifStmt->branches)
                    {
                        if (hasWaitStatements(branch.body))
                        {
                            return true;
                        }
                    }
                    if (hasWaitStatements(ifStmt->elseBody))
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    void SemanticAnalyzer::analyzeProcess(const ProcessStatement* proc)
    {
        if (!proc->label.empty())
        {
            if (m_processLabels.find(proc->label) != m_processLabels.end())
            {
                throw std::runtime_error("Semantic Error: Duplicate process label '" + proc->label + "'");
            }
            m_processLabels.insert(proc->label);
        }

        bool hasSensList = !proc->sensitivityList.empty();
        bool hasWait = hasWaitStatements(proc->body);

        if (hasSensList && hasWait)
        {
            throw std::runtime_error("Semantic Error: Process cannot have both a sensitivity list and wait statements.");
        }

        for (const auto& sigName : proc->sensitivityList)
        {
            if (m_currentScopeSymbols.find(sigName) == m_currentScopeSymbols.end())
            {
                throw std::runtime_error("Semantic Error: Undeclared signal '" + sigName + "' in process sensitivity list.");
            }
        }

        analyzeSequentialBody(proc->body);
    }

    void SemanticAnalyzer::analyzeSequentialBody(const std::vector<std::unique_ptr<SequentialStatement>>& body)
    {
        for (const auto& stmt : body)
        {
            analyzeSequentialStatement(stmt.get());
        }
    }

    void SemanticAnalyzer::analyzeSequentialStatement(const SequentialStatement* stmt)
    {
        if (auto asgn = dynamic_cast<const SignalAssignment*>(stmt))
        {
            analyzeAssignment(asgn);
        }
        else if (auto ifStmt = dynamic_cast<const IfStatement*>(stmt))
        {
            analyzeIfStatement(ifStmt);
        }
        else if (auto waitStmt = dynamic_cast<const WaitForStatement*>(stmt))
        {
            if (waitStmt->durationFs < 0)
            {
                throw std::runtime_error("Semantic Error: Wait duration cannot be negative.");
            }
        }
    }

    void SemanticAnalyzer::analyzeIfStatement(const IfStatement* ifStmt)
    {
        for (const auto& branch : ifStmt->branches)
        {
            analyzeCondition(branch.condition.get());
            analyzeSequentialBody(branch.body);
        }
        if (!ifStmt->elseBody.empty())
        {
            analyzeSequentialBody(ifStmt->elseBody);
        }
    }

    void SemanticAnalyzer::analyzeCondition(const Expression<ReturnType::BOOLEAN>* cond)
    {
        (void)cond;
    }
} // namespace Pulse::Parser::VHDL