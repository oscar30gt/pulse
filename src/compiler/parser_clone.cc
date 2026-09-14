#include "parser_internal.h"

namespace Pulse::Parser {
    
    TypeSpec cloneTypeSpec(const TypeSpec& src)
    {
        TypeSpec copy;
        copy.source = src.source;
        copy.typeName = src.typeName;
        copy.args.reserve(src.args.size());

        for (const auto& arg : src.args)
        {
            copy.args.push_back(cloneExpression(arg.get()));
        }
        return copy;
    }

    ExpressionPtr cloneExpression(const Expression* src)
    {
        if (!src) return nullptr;

        if (auto* ref = dynamic_cast<const SymbolExpr*>(src))
        {
            auto copy = std::make_unique<SymbolExpr>();
            copy->source = ref->source;
            copy->name = ref->name;
            return copy;
        }
        if (auto* bin = dynamic_cast<const BinaryOpExpr*>(src))
        {
            auto copy = std::make_unique<BinaryOpExpr>();
            copy->source = bin->source;
            copy->op = bin->op;
            copy->left = cloneExpression(bin->left.get());
            copy->right = cloneExpression(bin->right.get());
            return copy;
        }
        if (auto* un = dynamic_cast<const UnaryOpExpr*>(src))
        {
            auto copy = std::make_unique<UnaryOpExpr>();
            copy->source = un->source;
            copy->op = un->op;
            copy->operand = cloneExpression(un->operand.get());
            return copy;
        }
        if (auto* fn = dynamic_cast<const FunctionCallExpr*>(src))
        {
            auto copy = std::make_unique<FunctionCallExpr>();
            copy->source = fn->source;
            copy->functionName = fn->functionName;
            copy->arguments.reserve(fn->arguments.size());
            for (const auto& arg : fn->arguments)
            {
                copy->arguments.push_back(cloneExpression(arg.get()));
            }
            return copy;
        }
        if (auto* iLit = dynamic_cast<const IntegerLiteralExpr*>(src))
        {
            auto copy = std::make_unique<IntegerLiteralExpr>();
            copy->source = iLit->source;
            copy->value = iLit->value;
            return copy;
        }
        if (auto* bLit = dynamic_cast<const BooleanLiteral*>(src))
        {
            auto copy = std::make_unique<BooleanLiteral>();
            copy->source = bLit->source;
            copy->value = bLit->value;
            return copy;
        }
        if (auto* lLit = dynamic_cast<const LogicLiteralExpr*>(src))
        {
            auto copy = std::make_unique<LogicLiteralExpr>();
            copy->source = lLit->source;
            copy->value = lLit->value;
            copy->mask = lLit->mask;
            copy->width = lLit->width;
            copy->typeName = lLit->typeName;
            return copy;
        }
        if (auto* attr = dynamic_cast<const AttributeExpr*>(src))
        {
            auto copy = std::make_unique<AttributeExpr>();
            copy->source = attr->source;
            copy->attributeName = attr->attributeName;
            if (attr->target)
            {
                copy->target = std::unique_ptr<SymbolExpr>(
                    static_cast<SymbolExpr*>(cloneExpression(attr->target.get()).release())
                );
            }
            return copy;
        }
        if (auto* we = dynamic_cast<const WhenElseExpr*>(src))
        {
            auto copy = std::make_unique<WhenElseExpr>();
            copy->source = we->source;
            copy->condition = cloneExpression(we->condition.get());
            copy->trueValue = cloneExpression(we->trueValue.get());
            copy->falseValue = cloneExpression(we->falseValue.get());
            return copy;
        }

        throw std::runtime_error("Pulse Parser Error: Unknown Expression subtype in cloneExpression");
    }
} // namespace Pulse::Parser