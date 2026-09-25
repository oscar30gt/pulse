#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    namespace
    {
        /// A single element (character or enumeration literal), as opposed to a string or aggregate.
        bool isElementLiteral(const Expression& operand)
        {
            return dynamic_cast<const CharacterLiteralExpr*>(&operand) || dynamic_cast<const SymbolExpr*>(&operand);
        }

        /// How a design names a function that overloads an operator: the operator symbol between quotes.
        std::string operatorSymbol(const char* spelling)
        {
            return std::string("\"") + spelling + "\"";
        }
    } // anonymous namespace

    // ---- Design-declared operators --------------------------------------------------------------

    std::optional<SemanticType> AnalyzerContext::typeOfUserOperator(const Expression& node, const std::string& symbol,
                                                                    const std::vector<const Expression*>& operands,
                                                                    const SemanticType* expected)
    {
        const auto overloads = visibleSubprograms(symbol);
        if (overloads.empty())
            return std::nullopt;

        const bool anyFits = std::any_of(overloads.begin(), overloads.end(), [&](const SubprogramInfo* candidate)
        {
            return candidate->isFunction && candidate->parameters.size() == operands.size()
                && candidateProblem(*candidate, operands, node).empty();
        });

        // No overload accepts these operands: the predefined operators are still visible and get their turn.
        if (!anyFits)
            return std::nullopt;

        return resolveCall(symbol, node, operands, expected, true).returnType;
    }

    // ---- Unary ----------------------------------------------------------------------------------

    SemanticType AnalyzerContext::typeOfUnary(const UnaryOpExpr& expr, const SemanticType* expected)
    {
        if (auto user = typeOfUserOperator(expr, operatorSymbol(toString(expr.op)), { expr.operand.get() }, expected))
            return *user;

        const SemanticType operand = exprType(*expr.operand, expr.op == UnaryOperator::Not ? expected : nullptr);

        const RuleResult result = m_rules.unary(expr.op, operand);
        if (!result.ok())
            fail(result.problem, expr);

        return result.type;
    }

    // ---- Binary ---------------------------------------------------------------------------------

    SemanticType AnalyzerContext::typeOfBinary(const BinaryOpExpr& expr, const SemanticType* expected)
    {
        if (isRangeOperator(expr.op))
            fail(std::string("A range such as '") + toString(expr.op) + "' is not a value; it can only appear in an index, slice, loop or choice", expr);

        if (auto user = typeOfUserOperator(expr, operatorSymbol(toString(expr.op)), { expr.left.get(), expr.right.get() }, expected))
            return *user;

        const OperandTypes operands = typeOperands(expr, expected);

        const RuleResult result = m_rules.binary(expr.op, operands.left, operands.right, expected);
        if (!result.ok())
            fail(result.problem, expr);

        const bool division = expr.op == BinaryOperator::Div || expr.op == BinaryOperator::Mod || expr.op == BinaryOperator::Rem;
        if (division)
            if (auto divisor = fold(*expr.right); divisor && !divisor->isReal() && divisor->integer == 0)
                fail(std::string("Division by zero: the right operand of '") + toString(expr.op) + "' is 0", *expr.right);

        return result.type;
    }

    /// The type a literal operand must have, decided by its partner (or, alone, by the context).
    /// Returns an invalid type when nothing decides it.
    SemanticType AnalyzerContext::operandContext(const Expression& operand, const SemanticType& other, BinaryOperator op,
                                                 const SemanticType* expected) const
    {
        const bool concatenation = op == BinaryOperator::Concat;
        const bool wantsElement = isElementLiteral(operand);

        if (concatenation)
        {
            const SemanticType* array = isArray(other) ? &other : (!other.valid() && expected && isArray(*expected)) ? expected : nullptr;
            if (array)
                return wantsElement ? array->info->element : withoutConstraint(*array);

            // The partner is a single element: a string beside it must be the array type the context wants.
            if (other.valid() && !wantsElement && expected && isArray(*expected))
                return withoutConstraint(*expected);
            return other.valid() && wantsElement ? other : SemanticType{};
        }

        // A shifted literal is the left operand; the shift amount says nothing about its type.
        if (isShiftOperator(op))
            return expected ? *expected : SemanticType{};

        if (other.valid() && !isUniversal(other))
        {
            // Logical and matching operators need equal lengths, and an aggregate (`v = (others => '0')`) takes its index
            // range from the partner; a string literal compared or added elsewhere may have any length.
            const bool aggregate = dynamic_cast<const AggregateExpr*>(&operand) != nullptr;
            return isLogicalOperator(op) || isMatchingOperator(op) || aggregate ? other : withoutConstraint(other);
        }

        // Both operands lack a type of their own: relational results are boolean, the others take the context type.
        if (isRelationalOperator(op) || isMatchingOperator(op) || !expected)
            return {};
        return *expected;
    }

    /// Types both operands, giving a literal operand the type its partner (or the context) demands.
    OperandTypes AnalyzerContext::typeOperands(const BinaryOpExpr& expr, const SemanticType* expected)
    {
        const bool leftNeedsContext = needsContext(*expr.left);
        const bool rightNeedsContext = needsContext(*expr.right);

        // A chain `a & "01" & b` types its inner concatenations with the array type the context wants.
        const SemanticType concatContext = expr.op == BinaryOperator::Concat && expected && isArray(*expected)
                                         ? withoutConstraint(*expected) : SemanticType{};
        const SemanticType* selfContext = concatContext.valid() ? &concatContext : nullptr;

        const auto typeWithContext = [&](const Expression& operand, const SemanticType& other)
        {
            const SemanticType context = operandContext(operand, other, expr.op, expected);
            return exprType(operand, context.valid() ? &context : nullptr);
        };

        if (!leftNeedsContext && !rightNeedsContext)
            return { exprType(*expr.left, selfContext), exprType(*expr.right, selfContext) };

        if (leftNeedsContext && rightNeedsContext)
        {
            const SemanticType none;
            SemanticType left = typeWithContext(*expr.left, none);
            return { left, typeWithContext(*expr.right, left) };
        }

        if (leftNeedsContext)
        {
            SemanticType right = exprType(*expr.right, selfContext);
            return { typeWithContext(*expr.left, right), right };
        }

        SemanticType left = exprType(*expr.left, selfContext);
        return { left, typeWithContext(*expr.right, left) };
    }

} // namespace Pulse::Parser
