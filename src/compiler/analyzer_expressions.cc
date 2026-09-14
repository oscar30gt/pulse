#include "analyzer_internal.h"
#include <iostream>

namespace Pulse::Parser
{
    TypeSpec AnalyzerContext::exprType(const Expression* expr)
    {
        if (!expr)
        {
            throw std::runtime_error("Attempted to evaluate type of null expression.");
        }

        if (const auto* sym = dynamic_cast<const SymbolExpr*>(expr))
            return exprTypeSymbol(sym);
        if (const auto* intLit = dynamic_cast<const IntegerLiteralExpr*>(expr))
            return exprTypeIntLit(intLit);
        if (const auto* boolLit = dynamic_cast<const BooleanLiteral*>(expr))
            return exprTypeBoolLit(boolLit);
        if (const auto* logicLit = dynamic_cast<const LogicLiteralExpr*>(expr))
            return exprTypeLogicLit(logicLit);
        if (const auto* unOp = dynamic_cast<const UnaryOpExpr*>(expr))
            return exprTypeUnaryOp(unOp);
        if (const auto* binOp = dynamic_cast<const BinaryOpExpr*>(expr))
            return exprTypeBinaryOp(binOp);
        if (const auto* fnCall = dynamic_cast<const FunctionCallExpr*>(expr))
            return exprTypeFuncCall(fnCall);
        if (const auto* attr = dynamic_cast<const AttributeExpr*>(expr))
            return exprTypeAttribute(attr);
        if (const auto* whenElse = dynamic_cast<const WhenElseExpr*>(expr))
            return exprTypeWhenElse(whenElse);

        throw ast_semantic_error("Unknown expression node in type evaluation.", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeSymbol(const SymbolExpr* expr)
    {
        const SymbolInfo& info = getSymbol(expr->name, *expr);
        return cloneTypeSpec(info.typeSpec);
    }

    TypeSpec AnalyzerContext::exprTypeIntLit(const IntegerLiteralExpr* expr)
    {
        TypeSpec ts;
        ts.source = expr->source;
        ts.typeName = "integer";
        return ts;
    }

    TypeSpec AnalyzerContext::exprTypeBoolLit(const BooleanLiteral* expr)
    {
        TypeSpec ts;
        ts.source = expr->source;
        ts.typeName = "boolean";
        return ts;
    }

    TypeSpec AnalyzerContext::exprTypeLogicLit(const LogicLiteralExpr* expr)
    {
        if (expr->width == 1 && expr->typeName == "std_logic")
        {
            TypeSpec ts;
            ts.source = expr->source;
            ts.typeName = "std_logic";
            return ts;
        }
        std::string tName = expr->typeName.empty() ? "std_logic_vector" : expr->typeName;
        return makeVectorType(expr->width, tName, expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeUnaryOp(const UnaryOpExpr* expr)
    {
        TypeSpec operandType = exprType(expr->operand.get());

        if (expr->op == "not")
        {
            if (operandType.typeName == "boolean")
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "boolean";
                return res;
            }
            if (isLogicType(operandType.typeName))
            {
                return operandType;
            }
            throw ast_semantic_error("Unary 'not' operator requires boolean or logic operand.", expr->source);
        }

        if (expr->op == "-")
        {
            if (operandType.typeName == "integer" || operandType.typeName == "signed")
            {
                return operandType;
            }
            throw ast_semantic_error("Unary '-' operator requires integer or signed operand.", expr->source);
        }

        throw ast_semantic_error("Unknown unary operator '" + expr->op + "'", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeBinaryOp(const BinaryOpExpr* expr)
    {
        // 1. Ranges: downto, to
        if (expr->op == "downto" || expr->op == "to")
        {
            TypeSpec leftType = exprType(expr->left.get());
            TypeSpec rightType = exprType(expr->right.get());

            if (leftType.typeName != "integer" || rightType.typeName != "integer")
            {
                throw ast_semantic_error("Range bounds must be integers.", expr->source);
            }

            const auto* leftLit = dynamic_cast<const IntegerLiteralExpr*>(expr->left.get());
            const auto* rightLit = dynamic_cast<const IntegerLiteralExpr*>(expr->right.get());
            if (leftLit && rightLit)
            {
                if (expr->op == "downto" && leftLit->value < rightLit->value)
                {
                    throw ast_semantic_error("Invalid 'downto' range: left bound must be >= right bound.", expr->source);
                }
                if (expr->op == "to" && leftLit->value > rightLit->value)
                {
                    throw ast_semantic_error("Invalid 'to' range: left bound must be <= right bound.", expr->source);
                }
            }

            TypeSpec res;
            res.source = expr->source;
            res.typeName = "range";
            return res;
        }

        TypeSpec leftType = exprType(expr->left.get());
        TypeSpec rightType = exprType(expr->right.get());

        // 2. Arithmetic: +, -, *
        if (expr->op == "+" || expr->op == "-" || expr->op == "*")
        {
            if (leftType.typeName == "integer" && rightType.typeName == "integer")
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "integer";
                return res;
            }
            if (leftType.typeName == "signed" && rightType.typeName == "signed")
            {
                int wLeft = resolveVectorWidth(leftType);
                int wRight = resolveVectorWidth(rightType);
                if (wLeft != wRight || wLeft <= 0 || wRight <= 0)
                {
                    throw ast_semantic_error("Arithmetic operator '" + expr->op + "' requires operands of equal width.", expr->source);
                }
                return makeVectorType(wLeft, "signed", expr->source);
            }
            if (leftType.typeName == "unsigned" && rightType.typeName == "unsigned")
            {
                int wLeft = resolveVectorWidth(leftType);
                int wRight = resolveVectorWidth(rightType);
                if (wLeft != wRight || wLeft <= 0 || wRight <= 0)
                {
                    throw ast_semantic_error("Arithmetic operator '" + expr->op + "' requires operands of equal width.", expr->source);
                }
                return makeVectorType(wLeft, "unsigned", expr->source);
            }
            if (leftType.typeName == "signed" && rightType.typeName == "integer")
            {
                return leftType;
            }
            if (leftType.typeName == "integer" && rightType.typeName == "signed")
            {
                return rightType;
            }
            if (leftType.typeName == "unsigned" && rightType.typeName == "integer")
            {
                return leftType;
            }
            if (leftType.typeName == "integer" && rightType.typeName == "unsigned")
            {
                return rightType;
            }

            throw ast_semantic_error("Arithmetic operator '" + expr->op + "' requires integer, signed, or unsigned operands.", expr->source);
        }

        // 3. Logical: and, or, xor, nand, nor, xnor
        if (expr->op == "and" || expr->op == "or" || expr->op == "xor" ||
            expr->op == "nand" || expr->op == "nor" || expr->op == "xnor")
        {
            if (leftType.typeName == "boolean" && rightType.typeName == "boolean")
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "boolean";
                return res;
            }
            if (leftType.typeName == "std_logic" && rightType.typeName == "std_logic")
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "std_logic";
                return res;
            }
            if (isVectorType(leftType.typeName) && leftType.typeName == rightType.typeName)
            {
                if (!areTypesCompatible(leftType, rightType))
                {
                    throw ast_semantic_error("Logical operator '" + expr->op + "' requires operands of equal width.", expr->source);
                }
                return leftType;
            }
            throw ast_semantic_error("Logical operator '" + expr->op + "' requires boolean or matching vector operands of equal width.", expr->source);
        }

        // 4. Relational: =, /=, <, >, <=, >=
        if (expr->op == "=" || expr->op == "/=" || expr->op == "<" ||
            expr->op == ">" || expr->op == "<=" || expr->op == ">=")
        {
            if (leftType.typeName == "integer" && rightType.typeName == "integer")
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "boolean";
                return res;
            }
            if (leftType.typeName == "boolean" && rightType.typeName == "boolean")
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "boolean";
                return res;
            }
            if (leftType.typeName == "std_logic" && rightType.typeName == "std_logic")
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "boolean";
                return res;
            }
            if (isVectorType(leftType.typeName) && leftType.typeName == rightType.typeName)
            {
                if (!areTypesCompatible(leftType, rightType))
                {
                    throw ast_semantic_error("Relational operator '" + expr->op + "' requires operands of equal width.", expr->source);
                }
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "boolean";
                return res;
            }
            if ((leftType.typeName == "signed" && rightType.typeName == "integer") ||
                (leftType.typeName == "integer" && rightType.typeName == "signed") ||
                (leftType.typeName == "unsigned" && rightType.typeName == "integer") ||
                (leftType.typeName == "integer" && rightType.typeName == "unsigned"))
            {
                TypeSpec res;
                res.source = expr->source;
                res.typeName = "boolean";
                return res;
            }
            throw ast_semantic_error("Relational operator '" + expr->op + "' requires operands of comparable types.", expr->source);
        }

        // 5. Shift operators: sll, srl, sra, ror, rol
        if (expr->op == "sll" || expr->op == "srl" || expr->op == "sra" ||
            expr->op == "ror" || expr->op == "rol")
        {
            if (!isVectorType(leftType.typeName) && leftType.typeName != "std_logic")
            {
                throw ast_semantic_error("Shift operator '" + expr->op + "' requires a logic/vector left operand.", expr->source);
            }
            if (rightType.typeName != "integer")
            {
                throw ast_semantic_error("Shift operator '" + expr->op + "' requires an integer right operand.", expr->source);
            }
            return leftType;
        }

        // 6. Concatenation: &
        if (expr->op == "&")
        {
            if (!isLogicType(leftType.typeName) || !isLogicType(rightType.typeName))
            {
                throw ast_semantic_error("Concatenation operator '&' requires logic operands.", expr->source);
            }
            int wLeft = (leftType.typeName == "std_logic") ? 1 : resolveVectorWidth(leftType);
            int wRight = (rightType.typeName == "std_logic") ? 1 : resolveVectorWidth(rightType);
            std::string resType = (leftType.typeName == "signed" || leftType.typeName == "unsigned") ? leftType.typeName :
                                  ((rightType.typeName == "signed" || rightType.typeName == "unsigned") ? rightType.typeName : "std_logic_vector");
            if (wLeft != -1 && wRight != -1)
            {
                return makeVectorType(wLeft + wRight, resType, expr->source);
            }
            TypeSpec res;
            res.source = expr->source;
            res.typeName = resType;
            return res;
        }

        throw ast_semantic_error("Unknown binary operator '" + expr->op + "'", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeFuncCall(const FunctionCallExpr* expr)
    {
        if (expr->functionName == "unsigned")
            return exprTypeFuncUnsigned(expr);
        if (expr->functionName == "signed")
            return exprTypeFuncSigned(expr);
        if (expr->functionName == "to_unsigned")
            return exprTypeFuncToUnsigned(expr);
        if (expr->functionName == "to_signed")
            return exprTypeFuncToSigned(expr);
        if (expr->functionName == "to_integer")
            return exprTypeFuncToInteger(expr);
        if (expr->functionName == "rising_edge")
            return exprTypeFuncRisingEdge(expr);
        if (expr->functionName == "falling_edge")
            return exprTypeFuncFallingEdge(expr);
        if (expr->functionName == "std_logic_vector")
            return exprTypeFuncVectorCast(expr);

        // Check if function name is a signal / port symbol (range access or bit index)
        if (hasSymbol(expr->functionName))
        {
            return exprTypeSliceOrIndex(expr);
        }

        throw ast_semantic_error("Unknown function '" + expr->functionName + "'", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeFuncUnsigned(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 1)
        {
            throw ast_semantic_error("Function 'unsigned' expects 1 argument.", expr->source);
        }
        TypeSpec argType = exprType(expr->arguments[0].get());
        if (!isVectorType(argType.typeName) && argType.typeName != "std_logic")
        {
            throw ast_semantic_error("Function 'unsigned' argument must be of a vector or logic type.", expr->source);
        }
        int w = resolveVectorWidth(argType);
        return makeVectorType(w, "unsigned", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeFuncSigned(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 1)
        {
            throw ast_semantic_error("Function 'signed' expects 1 argument.", expr->source);
        }
        TypeSpec argType = exprType(expr->arguments[0].get());
        if (!isVectorType(argType.typeName) && argType.typeName != "std_logic")
        {
            throw ast_semantic_error("Function 'signed' argument must be of a vector or logic type.", expr->source);
        }
        int w = resolveVectorWidth(argType);
        return makeVectorType(w, "signed", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeFuncToUnsigned(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 2)
        {
            throw ast_semantic_error("Function 'to_unsigned' expects exactly 2 arguments (value, size).", expr->source);
        }
        TypeSpec valType = exprType(expr->arguments[0].get());
        if (valType.typeName != "integer")
        {
            throw ast_semantic_error("Function 'to_unsigned' first argument (value) must be of integer type.", expr->source);
        }
        TypeSpec sizeType = exprType(expr->arguments[1].get());
        if (sizeType.typeName != "integer")
        {
            throw ast_semantic_error("Function 'to_unsigned' second argument (size) must be of integer type.", expr->source);
        }
        const auto* sizeLit = dynamic_cast<const IntegerLiteralExpr*>(expr->arguments[1].get());
        if (sizeLit)
        {
            if (sizeLit->value <= 0)
            {
                throw ast_semantic_error("Function 'to_unsigned' size argument must be greater than 0.", expr->source);
            }
            return makeVectorType(static_cast<int>(sizeLit->value), "unsigned", expr->source);
        }
        TypeSpec res;
        res.source = expr->source;
        res.typeName = "unsigned";
        return res;
    }

    TypeSpec AnalyzerContext::exprTypeFuncToSigned(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 2)
        {
            throw ast_semantic_error("Function 'to_signed' expects exactly 2 arguments (value, size).", expr->source);
        }
        TypeSpec valType = exprType(expr->arguments[0].get());
        if (valType.typeName != "integer")
        {
            throw ast_semantic_error("Function 'to_signed' first argument (value) must be of integer type.", expr->source);
        }
        TypeSpec sizeType = exprType(expr->arguments[1].get());
        if (sizeType.typeName != "integer")
        {
            throw ast_semantic_error("Function 'to_signed' second argument (size) must be of integer type.", expr->source);
        }
        const auto* sizeLit = dynamic_cast<const IntegerLiteralExpr*>(expr->arguments[1].get());
        if (sizeLit)
        {
            if (sizeLit->value <= 0)
            {
                throw ast_semantic_error("Function 'to_signed' size argument must be greater than 0.", expr->source);
            }
            return makeVectorType(static_cast<int>(sizeLit->value), "signed", expr->source);
        }
        TypeSpec res;
        res.source = expr->source;
        res.typeName = "signed";
        return res;
    }

    TypeSpec AnalyzerContext::exprTypeFuncToInteger(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 1)
        {
            throw ast_semantic_error("Function 'to_integer' expects 1 argument.", expr->source);
        }
        TypeSpec argType = exprType(expr->arguments[0].get());
        if (!isVectorType(argType.typeName))
        {
            throw ast_semantic_error("Function 'to_integer' argument must be of a vector type (signed, unsigned, std_logic_vector).", expr->source);
        }
        TypeSpec res;
        res.source = expr->source;
        res.typeName = "integer";
        return res;
    }

    TypeSpec AnalyzerContext::exprTypeFuncRisingEdge(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 1)
        {
            throw ast_semantic_error("Function 'rising_edge' expects 1 argument.", expr->source);
        }
        TypeSpec argType = exprType(expr->arguments[0].get());
        if (argType.typeName != "std_logic")
        {
            throw ast_semantic_error("Function 'rising_edge' argument must be of type 'std_logic'.", expr->source);
        }
        TypeSpec res;
        res.source = expr->source;
        res.typeName = "boolean";
        return res;
    }

    TypeSpec AnalyzerContext::exprTypeFuncFallingEdge(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 1)
        {
            throw ast_semantic_error("Function 'falling_edge' expects 1 argument.", expr->source);
        }
        TypeSpec argType = exprType(expr->arguments[0].get());
        if (argType.typeName != "std_logic")
        {
            throw ast_semantic_error("Function 'falling_edge' argument must be of type 'std_logic'.", expr->source);
        }
        TypeSpec res;
        res.source = expr->source;
        res.typeName = "boolean";
        return res;
    }

    TypeSpec AnalyzerContext::exprTypeFuncVectorCast(const FunctionCallExpr* expr)
    {
        if (expr->arguments.size() != 1)
        {
            throw ast_semantic_error("Function 'std_logic_vector' expects 1 argument.", expr->source);
        }

        const auto* binOp = dynamic_cast<const BinaryOpExpr*>(expr->arguments[0].get());
        if (binOp && (binOp->op == "downto" || binOp->op == "to"))
        {
            TypeSpec ts;
            ts.source = expr->source;
            ts.typeName = "std_logic_vector";
            ts.args.push_back(cloneExpression(binOp));
            checkTypeSpec(ts);
            return ts;
        }

        TypeSpec argType = exprType(expr->arguments[0].get());
        if (isVectorType(argType.typeName) || argType.typeName == "std_logic")
        {
            int w = resolveVectorWidth(argType);
            return makeVectorType(w, "std_logic_vector", expr->source);
        }

        throw ast_semantic_error("Function 'std_logic_vector' argument must be a range, vector, or logic expression.", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeSliceOrIndex(const FunctionCallExpr* expr)
    {
        const SymbolInfo& sym = getSymbol(expr->functionName, *expr);
        if (!isVectorType(sym.typeSpec.typeName))
        {
            throw ast_semantic_error("Cannot index or slice non-vector symbol '" + expr->functionName + "'", expr->source);
        }

        if (expr->arguments.size() != 1)
        {
            throw ast_semantic_error("Indexing or slicing requires exactly 1 argument.", expr->source);
        }

        const auto* binOp = dynamic_cast<const BinaryOpExpr*>(expr->arguments[0].get());
        if (binOp && (binOp->op == "downto" || binOp->op == "to"))
        {
            // Slicing: sig(7 downto 0) -> preserves symbol vector type (signed, unsigned, std_logic_vector)
            TypeSpec rangeType = exprType(binOp);
            if (rangeType.typeName != "range")
            {
                throw ast_semantic_error("Invalid range in slice of '" + expr->functionName + "'", expr->source);
            }
            TypeSpec ts;
            ts.source = expr->source;
            ts.typeName = sym.typeSpec.typeName;
            ts.args.push_back(cloneExpression(binOp));
            return ts;
        }

        // Single index: sig(3) -> std_logic
        TypeSpec idxType = exprType(expr->arguments[0].get());
        if (idxType.typeName != "integer")
        {
            throw ast_semantic_error("Index expression must be of integer type.", expr->source);
        }

        TypeSpec res;
        res.source = expr->source;
        res.typeName = "std_logic";
        return res;
    }

    TypeSpec AnalyzerContext::exprTypeAttribute(const AttributeExpr* expr)
    {
        if (!expr->target)
        {
            throw ast_semantic_error("Attribute target must be specified.", expr->source);
        }

        const SymbolInfo& sym = getSymbol(expr->target->name, *expr);

        if (expr->attributeName == "length")
        {
            if (!isVectorType(sym.typeSpec.typeName))
            {
                throw ast_semantic_error("Attribute 'length' is only valid on vector types (std_logic_vector, signed, unsigned).", expr->source);
            }
            TypeSpec res;
            res.source = expr->source;
            res.typeName = "integer";
            return res;
        }

        if (expr->attributeName == "left" || expr->attributeName == "right" ||
            expr->attributeName == "high" || expr->attributeName == "low")
        {
            if (!isVectorType(sym.typeSpec.typeName))
            {
                throw ast_semantic_error("Attribute '" + expr->attributeName + "' is only valid on vector types (std_logic_vector, signed, unsigned).", expr->source);
            }
            TypeSpec res;
            res.source = expr->source;
            res.typeName = "std_logic";
            return res;
        }

        if (expr->attributeName == "event")
        {
            TypeSpec res;
            res.source = expr->source;
            res.typeName = "boolean";
            return res;
        }

        throw ast_semantic_error("Unknown attribute '" + expr->attributeName + "'", expr->source);
    }

    TypeSpec AnalyzerContext::exprTypeWhenElse(const WhenElseExpr* expr)
    {
        if (expr->condition)
        {
            TypeSpec condType = exprType(expr->condition.get());
            if (condType.typeName != "boolean")
            {
                throw ast_semantic_error("'when' condition must be of type 'boolean'.", expr->source);
            }
        }

        TypeSpec trueType = exprType(expr->trueValue.get());

        if (expr->falseValue)
        {
            TypeSpec falseType = exprType(expr->falseValue.get());
            if (!areTypesCompatible(trueType, falseType))
            {
                throw ast_semantic_error("Branches of when-else expression must produce compatible types.", expr->source);
            }
        }

        return trueType;
    }

} // namespace Pulse::Parser
