#include "analyzer_internal.h"

namespace Pulse::Parser
{
    bool AnalyzerContext::isBuiltinType(const std::string& typeName) const
    {
        return typeName == "std_logic" ||
               typeName == "std_logic_vector" ||
               typeName == "signed" ||
               typeName == "unsigned" ||
               typeName == "integer" ||
               typeName == "boolean" ||
               typeName == "range";
    }

    bool AnalyzerContext::isVectorType(const std::string& typeName) const
    {
        return typeName == "std_logic_vector" ||
               typeName == "signed" ||
               typeName == "unsigned";
    }

    bool AnalyzerContext::isLogicType(const std::string& typeName) const
    {
        return typeName == "std_logic" || isVectorType(typeName);
    }

    bool AnalyzerContext::areBothLogic(const TypeSpec& a, const TypeSpec& b) const
    {
        return isLogicType(a.typeName) && isLogicType(b.typeName);
    }

    TypeSpec AnalyzerContext::makeVectorType(int width, const std::string& typeName, SourceLocation loc) const
    {
        TypeSpec ts;
        ts.source = loc;
        ts.typeName = typeName;

        if (width > 0)
        {
            auto binOp = std::make_unique<BinaryOpExpr>();
            binOp->source = loc;
            binOp->op = "downto";

            auto left = std::make_unique<IntegerLiteralExpr>();
            left->source = loc;
            left->value = width - 1;

            auto right = std::make_unique<IntegerLiteralExpr>();
            right->source = loc;
            right->value = 0;

            binOp->left = std::move(left);
            binOp->right = std::move(right);

            ts.args.push_back(std::move(binOp));
        }

        return ts;
    }

    int AnalyzerContext::resolveVectorWidth(const TypeSpec& typeSpec) const
    {
        if (typeSpec.typeName == "std_logic")
        {
            return 1;
        }

        if (!isVectorType(typeSpec.typeName))
        {
            return -1;
        }

        if (typeSpec.args.empty())
        {
            return -1;
        }

        const auto* binOp = dynamic_cast<const BinaryOpExpr*>(typeSpec.args[0].get());
        if (!binOp)
        {
            return -1;
        }

        const auto* leftLit = dynamic_cast<const IntegerLiteralExpr*>(binOp->left.get());
        const auto* rightLit = dynamic_cast<const IntegerLiteralExpr*>(binOp->right.get());
        if (!leftLit || !rightLit)
        {
            return -1;
        }

        if (binOp->op == "downto")
        {
            if (leftLit->value >= rightLit->value)
            {
                return static_cast<int>(leftLit->value - rightLit->value + 1);
            }
        }
        else if (binOp->op == "to")
        {
            if (rightLit->value >= leftLit->value)
            {
                return static_cast<int>(rightLit->value - leftLit->value + 1);
            }
        }

        return -1;
    }

    void AnalyzerContext::checkTypeSpec(const TypeSpec& typeSpec)
    {
        if (!isBuiltinType(typeSpec.typeName))
        {
            throw ast_semantic_error("Unknown type '" + typeSpec.typeName + "'", typeSpec.source);
        }

        if (typeSpec.typeName == "std_logic")
        {
            if (!typeSpec.args.empty())
            {
                throw ast_semantic_error("Type 'std_logic' takes no arguments", typeSpec.source);
            }
            return;
        }

        if (typeSpec.typeName == "integer")
        {
            if (!typeSpec.args.empty())
            {
                throw ast_semantic_error("Type 'integer' takes no arguments", typeSpec.source);
            }
            return;
        }

        if (typeSpec.typeName == "boolean")
        {
            if (!typeSpec.args.empty())
            {
                throw ast_semantic_error("Type 'boolean' takes no arguments", typeSpec.source);
            }
            return;
        }

        if (isVectorType(typeSpec.typeName))
        {
            if (typeSpec.args.size() != 1)
            {
                throw ast_semantic_error("Type '" + typeSpec.typeName + "' requires exactly one range argument", typeSpec.source);
            }

            const auto* binOp = dynamic_cast<const BinaryOpExpr*>(typeSpec.args[0].get());
            if (!binOp || (binOp->op != "downto" && binOp->op != "to"))
            {
                throw ast_semantic_error("'" + typeSpec.typeName + "' argument must be a range expression", typeSpec.source);
            }

            TypeSpec leftType = const_cast<AnalyzerContext*>(this)->exprType(binOp->left.get());
            TypeSpec rightType = const_cast<AnalyzerContext*>(this)->exprType(binOp->right.get());

            if (leftType.typeName != "integer" || rightType.typeName != "integer")
            {
                throw ast_semantic_error("Range bounds must be integers", binOp->source);
            }

            const auto* leftLit = dynamic_cast<const IntegerLiteralExpr*>(binOp->left.get());
            const auto* rightLit = dynamic_cast<const IntegerLiteralExpr*>(binOp->right.get());
            if (leftLit && rightLit)
            {
                if (binOp->op == "downto" && leftLit->value < rightLit->value)
                {
                    throw ast_semantic_error("Invalid 'downto' range: left bound must be >= right bound", binOp->source);
                }
                if (binOp->op == "to" && leftLit->value > rightLit->value)
                {
                    throw ast_semantic_error("Invalid 'to' range: left bound must be <= right bound", binOp->source);
                }
            }
        }
    }

    bool AnalyzerContext::areTypesCompatible(const TypeSpec& left, const TypeSpec& right) const
    {
        if (left.typeName != right.typeName)
        {
            return false;
        }

        if (isVectorType(left.typeName))
        {
            int wLeft = resolveVectorWidth(left);
            int wRight = resolveVectorWidth(right);
            if (wLeft != -1 && wRight != -1 && wLeft != wRight)
            {
                return false;
            }
        }

        return true;
    }

    bool AnalyzerContext::isLiteralCompatible(const TypeSpec& targetType, const Expression* valueExpr) const
    {
        if (!valueExpr) return false;

        if (const auto* logicLit = dynamic_cast<const LogicLiteralExpr*>(valueExpr))
        {
            if ((targetType.typeName == "signed" || targetType.typeName == "unsigned") &&
                logicLit->typeName == "std_logic_vector")
            {
                int targetWidth = resolveVectorWidth(targetType);
                if (targetWidth == -1 || targetWidth == static_cast<int>(logicLit->width))
                {
                    return true;
                }
            }
        }

        return false;
    }

} // namespace Pulse::Parser
