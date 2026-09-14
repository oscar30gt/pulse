#include "analyzer_internal.h"
#include <unordered_set>

namespace Pulse::Parser
{
    AnalyzerContext::AnalyzerContext()
    {
    }

    void AnalyzerContext::collectEntities(const ASTRoot& root)
    {
        m_entities.clear();
        for (const auto& child : root.children)
        {
            if (const auto* entity = dynamic_cast<const EntityDeclaration*>(child.get()))
            {
                if (m_entities.find(entity->name) != m_entities.end())
                {
                    throw ast_semantic_error("Entity '" + entity->name + "' already declared.", entity->source);
                }
                m_entities[entity->name] = entity;
            }
        }
    }

    void AnalyzerContext::analyzeEntity(const EntityDeclaration& entity)
    {
        std::unordered_set<std::string> portNames;

        for (const auto& port : entity.ports)
        {
            if (portNames.find(port.portName) != portNames.end())
            {
                throw ast_semantic_error("Port '" + port.portName + "' already declared in entity '" + entity.name + "'.", port.source);
            }
            portNames.insert(port.portName);

            checkTypeSpec(port.typeSpec);
        }
    }

    static bool isCompileTimeConstant(const Expression* expr)
    {
        if (!expr) return true;

        if (dynamic_cast<const IntegerLiteralExpr*>(expr) ||
            dynamic_cast<const BooleanLiteral*>(expr) ||
            dynamic_cast<const LogicLiteralExpr*>(expr))
        {
            return true;
        }

        if (const auto* unOp = dynamic_cast<const UnaryOpExpr*>(expr))
        {
            return isCompileTimeConstant(unOp->operand.get());
        }

        if (const auto* binOp = dynamic_cast<const BinaryOpExpr*>(expr))
        {
            return isCompileTimeConstant(binOp->left.get()) && isCompileTimeConstant(binOp->right.get());
        }

        if (const auto* fn = dynamic_cast<const FunctionCallExpr*>(expr))
        {
            for (const auto& arg : fn->arguments)
            {
                if (!isCompileTimeConstant(arg.get())) return false;
            }
            return true;
        }

        return false;
    }

    void AnalyzerContext::checkInitialValue(const Expression* init, const TypeSpec& targetType, const ASTNode& node)
    {
        if (!init) return;

        if (!isCompileTimeConstant(init))
        {
            throw ast_semantic_error("Signal initial value must be a compile-time constant.", init->source);
        }

        TypeSpec initType = exprType(init);
        if (!areTypesCompatible(targetType, initType))
        {
            throw ast_semantic_error("Initial value type does not match signal type.", node.source);
        }
    }

    void AnalyzerContext::checkSignalDeclaration(const SignalDeclaration& signal)
    {
        checkTypeSpec(signal.typeSpec);

        if (signal.initialValue)
        {
            checkInitialValue(signal.initialValue.get(), signal.typeSpec, signal);
        }
    }

    void AnalyzerContext::analyzeArchitecture(const ArchitectureDeclaration& arch)
    {
        auto entityIt = m_entities.find(arch.entityName);
        if (entityIt == m_entities.end())
        {
            throw ast_semantic_error("Architecture '" + arch.name + "' references unknown entity '" + arch.entityName + "'.", arch.source);
        }

        m_components.clear();
        pushScope();

        // 1. Register entity ports in architecture scope
        const EntityDeclaration* entity = entityIt->second;
        for (const auto& port : entity->ports)
        {
            SymbolKind kind = (port.isInput && port.isOutput) ? SymbolKind::IOPort : (port.isInput ? SymbolKind::IPort : SymbolKind::OPort);
            declareSymbol(port.portName, kind, port.typeSpec, port);
        }

        // 2. Register components
        for (const auto& comp : arch.components)
        {
            if (m_components.find(comp.name) != m_components.end())
            {
                throw ast_semantic_error("Component '" + comp.name + "' already declared in this architecture.", comp.source);
            }
            for (const auto& port : comp.ports)
            {
                checkTypeSpec(port.typeSpec);
            }
            m_components[comp.name] = &comp;
        }

        // 3. Register signals
        for (const auto& signal : arch.signals)
        {
            checkSignalDeclaration(signal);
            declareSymbol(signal.name, SymbolKind::Signal, signal.typeSpec, signal);
        }

        // 4. Analyze concurrent statements
        for (const auto& stmt : arch.body)
        {
            analyzeConcurrentStmt(*stmt);
        }

        popScope();
    }

    void AnalyzerContext::analyze(const ASTRoot& root)
    {
        collectEntities(root);

        // Analyze all entity declarations
        for (const auto& child : root.children)
        {
            if (const auto* entity = dynamic_cast<const EntityDeclaration*>(child.get()))
            {
                analyzeEntity(*entity);
            }
        }

        // Analyze all architecture declarations
        for (const auto& child : root.children)
        {
            if (const auto* arch = dynamic_cast<const ArchitectureDeclaration*>(child.get()))
            {
                analyzeArchitecture(*arch);
            }
        }
    }

    // --------------------------------------------------------------------------------------------

    void analyzeAST(const ASTRoot& root)
    {
        AnalyzerContext ctx;
        ctx.analyze(root);
    }

} // namespace Pulse::Parser