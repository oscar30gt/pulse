#include "operator_helpers.h"

#include <algorithm>

namespace Pulse::Parser
{
    namespace
    {
        /// The type two numeric operands share; a universal operand adopts the other operand's type.
        std::optional<SemanticType> unifyNumeric(const SemanticType& l, const SemanticType& r)
        {
            if (isUniversal(l) && isUniversal(r))
                return isUniversalInteger(l) == isUniversalInteger(r) ? std::optional<SemanticType>(l) : std::nullopt;

            if (isUniversal(l) || isUniversal(r))
            {
                const SemanticType& universal = isUniversal(l) ? l : r;
                const SemanticType& typed = isUniversal(l) ? r : l;
                const bool fits = isUniversalInteger(universal) ? isIntegerClass(typed) : isRealClass(typed);
                return fits ? std::optional<SemanticType>(baseResult(typed)) : std::nullopt;
            }

            return l.info == r.info ? std::optional<SemanticType>(baseResult(l)) : std::nullopt;
        }

        bool isIntegerOrReal(const SemanticType& type) { return isIntegerClass(type) || isRealClass(type); }
    } // anonymous namespace

    // ---- Dispatch -------------------------------------------------------------------------------

    RuleResult OperatorRules::binary(BinaryOperator op, const SemanticType& l, const SemanticType& r,
                                     const SemanticType* expected) const
    {
        if (isLogicalOperator(op)) return logical(op, l, r);
        if (isShiftOperator(op)) return shift(op, l, r);
        if (isMatchingOperator(op)) return matching(op, l, r);
        if (op == BinaryOperator::Concat) return concatenate(l, r, expected);

        const bool vectorOperand = isNumericVector(l) || isNumericVector(r);
        if (op == BinaryOperator::Eq || op == BinaryOperator::Neq)
            return vectorOperand ? vectorComparison(op, l, r) : equality(op, l, r);
        if (isRelationalOperator(op))
            return vectorOperand ? vectorComparison(op, l, r) : ordering(op, l, r);

        if (vectorOperand)
            return numericVector(op, l, r);

        switch (op)
        {
            case BinaryOperator::Add: case BinaryOperator::Sub:
                return additive(op, l, r);
            case BinaryOperator::Mul: case BinaryOperator::Div: case BinaryOperator::Mod: case BinaryOperator::Rem:
                return multiplicative(op, l, r);
            case BinaryOperator::Pow:
                return exponent(l, r);
            default:
                return RuleResult::failure(std::string("The range operator '") + toString(op) + "' is not a value operator");
        }
    }

    RuleResult OperatorRules::unary(UnaryOperator op, const SemanticType& operand) const
    {
        switch (op)
        {
            case UnaryOperator::Not:       return logicalNot(operand);
            case UnaryOperator::Condition: return condition(operand);
            case UnaryOperator::Plus: case UnaryOperator::Minus: case UnaryOperator::Abs:
                return signedUnary(op, operand);
            default:
                return reduction(op, operand);
        }
    }

    // ---- Numeric --------------------------------------------------------------------------------

    RuleResult OperatorRules::additive(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        if (!isNumeric(l) || !isNumeric(r))
        {
            const bool logicVector = (isArray(l) && l.info->family == VectorFamily::StdLogicVector)
                                  || (isArray(r) && r.info->family == VectorFamily::StdLogicVector);
            return RuleResult::failure(operatorProblem(toString(op), l, r, logicVector
                ? "std_logic_vector has no arithmetic; convert the operand to unsigned or signed first, e.g. unsigned(a)"
                : "arithmetic needs numeric operands"));
        }

        if (isPhysicalType(l) || isPhysicalType(r))
        {
            return l.info == r.info
                ? RuleResult::success(baseResult(l))
                : RuleResult::failure(operatorProblem(toString(op), l, r, "physical values can only be added to or subtracted from values of the same type"));
        }

        auto unified = unifyNumeric(l, r);
        return unified ? RuleResult::success(*unified)
                       : RuleResult::failure(operatorProblem(toString(op), l, r, "both operands must have the same type"));
    }

    RuleResult OperatorRules::multiplicative(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        if (!isNumeric(l) || !isNumeric(r))
            return RuleResult::failure(operatorProblem(toString(op), l, r, "arithmetic needs numeric operands"));

        if (op == BinaryOperator::Mod || op == BinaryOperator::Rem)
        {
            auto unified = isIntegerClass(l) && isIntegerClass(r) ? unifyNumeric(l, r) : std::nullopt;
            return unified ? RuleResult::success(*unified)
                           : RuleResult::failure(operatorProblem(toString(op), l, r, std::string("'") + toString(op) + "' needs two integers of the same type"));
        }

        const bool physicalLeft = isPhysicalType(l);
        const bool physicalRight = isPhysicalType(r);

        if (physicalLeft && physicalRight)
        {
            return op == BinaryOperator::Div && l.info == r.info
                ? RuleResult::success(universalInteger())
                : RuleResult::failure(operatorProblem(toString(op), l, r, "two physical values can only be divided, and only by the same type"));
        }

        if (physicalLeft && isIntegerOrReal(r)) return RuleResult::success(baseResult(l));
        if (physicalRight && op == BinaryOperator::Mul && isIntegerOrReal(l)) return RuleResult::success(baseResult(r));
        if (physicalLeft || physicalRight)
            return RuleResult::failure(operatorProblem(toString(op), l, r, "a physical value can only be multiplied or divided by a number"));

        if (isUniversal(l) && isUniversal(r) && isUniversalInteger(l) != isUniversalInteger(r))
            return RuleResult::success(universalReal());

        auto unified = unifyNumeric(l, r);
        return unified ? RuleResult::success(*unified)
                       : RuleResult::failure(operatorProblem(toString(op), l, r, "both operands must have the same type"));
    }

    RuleResult OperatorRules::exponent(const SemanticType& l, const SemanticType& r) const
    {
        if (!isIntegerOrReal(l) || !isIntegerClass(r))
            return RuleResult::failure(operatorProblem(toString(BinaryOperator::Pow), l, r, "the base must be an integer or real and the exponent an integer"));

        return RuleResult::success(baseResult(l));
    }

    RuleResult OperatorRules::signedUnary(UnaryOperator op, const SemanticType& operand) const
    {
        if (isNumeric(operand))
            return RuleResult::success(baseResult(operand));

        if (isArray(operand) && operand.info->family == VectorFamily::Signed && (op == UnaryOperator::Minus || op == UnaryOperator::Abs))
            return RuleResult::success(operand);

        return RuleResult::failure(operatorProblem(toString(op), operand, "it needs a numeric operand"));
    }

    // ---- Relational -----------------------------------------------------------------------------

    RuleResult OperatorRules::equality(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        const bool comparable = (isNumeric(l) && isNumeric(r)) ? unifyNumeric(l, r).has_value() : l.info == r.info;
        if (!comparable)
            return RuleResult::failure(operatorProblem(toString(op), l, r, "both operands must have the same type"));

        return RuleResult::success(typeOf(*m_boolean));
    }

    RuleResult OperatorRules::ordering(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        const auto orderable = [](const SemanticType& t)
        {
            return isScalar(t) || (isOneDimensionalArray(t) && isDiscrete(t.info->element));
        };

        if (!orderable(l) || !orderable(r))
            return RuleResult::failure(operatorProblem(toString(op), l, r, "only scalar values and arrays of scalars can be ordered"));

        return equality(op, l, r);
    }

} // namespace Pulse::Parser
