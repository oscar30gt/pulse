#include "analyzer_internal.h"
#include <unordered_set>

namespace Pulse::Parser
{
    bool AnalyzerContext::isAttributeModifiable(const std::string& attributeName) const
    {
        // Extensibility hook: currently all built-in attributes ('length, 'left, 'right, 'high, 'low, 'event)
        // are read-only in VHDL. If modifiable attributes are added in the future, list them here.
        static const std::unordered_set<std::string> writableAttributes = {};
        return writableAttributes.count(attributeName) > 0;
    }

    bool AnalyzerContext::isLvalue(const Expression* expr) const
    {
        if (!expr)
        {
            return false;
        }

        // Direct symbol reference
        if (const auto* sym = dynamic_cast<const SymbolExpr*>(expr))
        {
            const SymbolInfo& info = getSymbol(sym->name, *sym);
            if (info.kind == SymbolKind::IPort)
            {
                throw ast_semantic_error("Cannot assign to input port '" + sym->name + "'.", sym->source);
            }
            return info.kind == SymbolKind::Signal ||
                   info.kind == SymbolKind::OPort ||
                   info.kind == SymbolKind::IOPort;
        }

        // Slicing or bit-indexing: e.g. sig(7 downto 0) <= ... or sig(0) <= ...
        if (const auto* fn = dynamic_cast<const FunctionCallExpr*>(expr))
        {
            if (hasSymbol(fn->functionName))
            {
                const SymbolInfo& info = getSymbol(fn->functionName, *fn);
                if (info.kind == SymbolKind::IPort)
                {
                    throw ast_semantic_error("Cannot assign to input port '" + fn->functionName + "'.", fn->source);
                }
                return info.kind == SymbolKind::Signal ||
                       info.kind == SymbolKind::OPort ||
                       info.kind == SymbolKind::IOPort;
            }
            return false;
        }

        // Attribute access: e.g. sig'left
        if (const auto* attr = dynamic_cast<const AttributeExpr*>(expr))
        {
            if (!isAttributeModifiable(attr->attributeName))
            {
                throw ast_semantic_error("Attribute '" + attr->attributeName + "' is read-only.", attr->source);
            }
            return true;
        }

        return false;
    }

    void AnalyzerContext::analyzeAssignment(const Expression* left, const Expression* right, const ASTNode& node)
    {
        if (!left)
        {
            throw ast_semantic_error("Missing assignment target.", node.source);
        }
        if (!right)
        {
            throw ast_semantic_error("Missing assignment value expression.", node.source);
        }

        if (!isLvalue(left))
        {
            throw ast_semantic_error("Left-hand side of assignment is not a modifiable signal or port.", left->source);
        }

        TypeSpec leftType = exprType(left);
        TypeSpec rightType = exprType(right);

        if (!areTypesCompatible(leftType, rightType) && !isLiteralCompatible(leftType, right))
        {
            int wLeft = resolveVectorWidth(leftType);
            int wRight = resolveVectorWidth(rightType);
            if (isVectorType(leftType.typeName) && isVectorType(rightType.typeName) &&
                leftType.typeName == rightType.typeName &&
                wLeft != -1 && wRight != -1 && wLeft != wRight)
            {
                throw ast_semantic_error("Width mismatch in assignment: target is " + std::to_string(wLeft) +
                                         " bits, but value is " + std::to_string(wRight) + " bits.", node.source);
            }

            throw ast_semantic_error("Type mismatch: cannot assign '" + rightType.typeName + "' to '" + leftType.typeName + "'.", node.source);
        }
    }

} // namespace Pulse::Parser
