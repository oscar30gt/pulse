#include "analyzer_internal.h"

namespace Pulse::Parser
{
    void AnalyzerContext::analyzeConcurrentStmt(const Statement& stmt)
    {
        if (const auto* assign = dynamic_cast<const SignalAssignment*>(&stmt))
        {
            analyzeSignalAssign(*assign);
        }
        else if (const auto* withClause = dynamic_cast<const WithClause*>(&stmt))
        {
            analyzeWithClause(*withClause);
        }
        else if (const auto* inst = dynamic_cast<const ComponentInstantiation*>(&stmt))
        {
            analyzeCompInstantiation(*inst);
        }
        else if (const auto* proc = dynamic_cast<const ProcessStatement*>(&stmt))
        {
            analyzeProcess(*proc);
        }
        else
        {
            throw ast_semantic_error("Unknown concurrent statement in architecture body.", stmt.source);
        }
    }

    void AnalyzerContext::analyzeSignalAssign(const SignalAssignment& assign)
    {
        analyzeAssignment(assign.target.get(), assign.value.get(), assign);
    }

    void AnalyzerContext::analyzeWithClause(const WithClause& stmt)
    {
        if (!stmt.selector)
        {
            throw ast_semantic_error("With-select statement missing selector expression.", stmt.source);
        }

        TypeSpec selectorType = exprType(stmt.selector.get());

        if (stmt.choices.empty())
        {
            throw ast_semantic_error("With-select statement has no choices.", stmt.source);
        }

        TypeSpec targetType;
        bool hasTargetType = false;

        for (size_t i = 0; i < stmt.choices.size(); ++i)
        {
            const auto& [condition, value] = stmt.choices[i];

            // Condition must match selector type
            if (condition)
            {
                TypeSpec condType = exprType(condition.get());
                if (!areTypesCompatible(selectorType, condType))
                {
                    throw ast_semantic_error("Choice condition type does not match selector type in with-select.", condition->source);
                }
            }

            if (!value)
            {
                continue;
            }

            // In VHDL AST from parser: first choice might be "target <= val1"
            if (const auto* binOp = dynamic_cast<const BinaryOpExpr*>(value.get()))
            {
                if (binOp->op == "<=")
                {
                    if (!isLvalue(binOp->left.get()))
                    {
                        throw ast_semantic_error("Left-hand side of assignment in with-select is not a modifiable signal.", binOp->left->source);
                    }
                    targetType = exprType(binOp->left.get());
                    hasTargetType = true;

                    TypeSpec valType = exprType(binOp->right.get());
                    if (!areTypesCompatible(targetType, valType) && !isLiteralCompatible(targetType, binOp->right.get()))
                    {
                        throw ast_semantic_error("Choice value type does not match target type in with-select.", binOp->right->source);
                    }
                    continue;
                }
            }

            TypeSpec valType = exprType(value.get());
            if (!hasTargetType)
            {
                targetType = std::move(valType);
                hasTargetType = true;
            }
            else
            {
                if (!areTypesCompatible(targetType, valType) && !isLiteralCompatible(targetType, value.get()))
                {
                    throw ast_semantic_error("Choice value type does not match expected target type in with-select.", value->source);
                }
            }
        }

        if (stmt.defaultValue)
        {
            TypeSpec defType = exprType(stmt.defaultValue.get());
            if (hasTargetType && !areTypesCompatible(targetType, defType) && !isLiteralCompatible(targetType, stmt.defaultValue.get()))
            {
                throw ast_semantic_error("Default choice ('others') value type does not match target type in with-select.", stmt.defaultValue->source);
            }
        }
    }

    void AnalyzerContext::analyzeCompInstantiation(const ComponentInstantiation& inst)
    {
        declareTag(inst.instanceName, inst);

        auto compIt = m_components.find(inst.componentName);
        if (compIt == m_components.end())
        {
            throw ast_semantic_error("Unknown component '" + inst.componentName + "' in instantiation.", inst.source);
        }

        const ComponentDeclaration* comp = compIt->second;

        for (const auto& [formalPort, actualRef] : inst.portMap)
        {
            bool formalExists = false;
            for (const auto& p : comp->ports)
            {
                if (p.portName == formalPort)
                {
                    formalExists = true;
                    break;
                }
            }

            if (!formalExists)
            {
                throw ast_semantic_error("Component '" + inst.componentName + "' has no port named '" + formalPort + "'.", inst.source);
            }

            // Verify actual signal exists in current scope
            getSymbol(actualRef.name, actualRef);
        }
    }

    void AnalyzerContext::analyzeProcess(const ProcessStatement& proc)
    {
        if (!proc.label.empty())
        {
            declareTag(proc.label, proc);
        }

        for (const auto& sigName : proc.sensitivityList)
        {
            SymbolExpr dummy;
            dummy.name = sigName;
            dummy.source = proc.source;
            getSymbol(sigName, dummy);
        }

        if (!proc.sensitivityList.empty())
        {
            checkNoWaitStatements(proc.body, proc);
        }

        pushScope();

        for (const auto& stmt : proc.body)
        {
            analyzeSeqStmt(*stmt);
        }

        popScope();
    }

    void AnalyzerContext::checkNoWaitStatements(const std::vector<std::unique_ptr<SequentialStatement>>& body, const ASTNode& proc)
    {
        for (const auto& stmt : body)
        {
            if (dynamic_cast<const WaitForStatement*>(stmt.get()) ||
                dynamic_cast<const WaitForeverStatement*>(stmt.get()))
            {
                throw ast_semantic_error("A process with a sensitivity list cannot contain wait statements.", stmt->source);
            }

            if (const auto* ifStmt = dynamic_cast<const IfStatement*>(stmt.get()))
            {
                for (const auto& branch : ifStmt->branches)
                {
                    checkNoWaitStatements(branch.body, proc);
                }
                checkNoWaitStatements(ifStmt->elseBody, proc);
            }
        }
    }

    void AnalyzerContext::analyzeSeqStmt(const Statement& stmt)
    {
        if (const auto* assign = dynamic_cast<const SignalAssignment*>(&stmt))
        {
            analyzeSignalAssign(*assign);
        }
        else if (const auto* ifStmt = dynamic_cast<const IfStatement*>(&stmt))
        {
            analyzeIfStmt(*ifStmt);
        }
        else if (dynamic_cast<const WaitForStatement*>(&stmt) ||
                 dynamic_cast<const WaitForeverStatement*>(&stmt))
        {
            // Wait statements are valid in processes without a sensitivity list
        }
        else
        {
            throw ast_semantic_error("Unknown sequential statement in process body.", stmt.source);
        }
    }

    void AnalyzerContext::analyzeIfStmt(const IfStatement& stmt)
    {
        for (const auto& branch : stmt.branches)
        {
            if (!branch.condition)
            {
                throw ast_semantic_error("If branch missing condition.", stmt.source);
            }

            TypeSpec condType = exprType(branch.condition.get());
            if (condType.typeName != "boolean")
            {
                throw ast_semantic_error("Condition in if/elsif statement must be of type 'boolean'.", branch.condition->source);
            }

            for (const auto& s : branch.body)
            {
                analyzeSeqStmt(*s);
            }
        }

        for (const auto& s : stmt.elseBody)
        {
            analyzeSeqStmt(*s);
        }
    }

} // namespace Pulse::Parser
