#include "operator_helpers.h"

#include <algorithm>

namespace Pulse::Parser
{
    namespace
    {
        /// `len-1 downto 0` (or `0 to len-1` when `ascending`); no bounds when the length is unknown.
        SemanticType arrayOfLength(const TypeInfo& info, std::optional<int64_t> length, bool ascending = false)
        {
            SemanticType type;
            type.info = &info;
            if (length && *length > 0)
                type.dims = { ascending ? Bounds{ 0, *length - 1, true } : Bounds{ *length - 1, 0, false } };
            return type;
        }

        std::optional<int64_t> sumOf(std::optional<int64_t> a, std::optional<int64_t> b)
        {
            return a && b ? std::optional<int64_t>(*a + *b) : std::nullopt;
        }

        bool ascendingDimension(const SemanticType& type)
        {
            return !type.dims.empty() && type.dims.front().ascending;
        }
    } // anonymous namespace

    // ---- Logical operators and shifts -----------------------------------------------------------

    RuleResult OperatorRules::logical(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        if (!isLogicalOperand(l, m_boolean) || !isLogicalOperand(r, m_boolean))
            return RuleResult::failure(operatorProblem(toString(op), l, r, "logical operators work on boolean, std_logic and arrays of them"));

        if (l.info != r.info)
            return RuleResult::failure(operatorProblem(toString(op), l, r, "both operands must have the same type"));

        const auto leftLength = staticLength(l);
        const auto rightLength = staticLength(r);
        if (leftLength && rightLength && *leftLength != *rightLength)
            return RuleResult::failure(operatorProblem(toString(op), l, r, "the operands must have the same length ("
                                                       + std::to_string(*leftLength) + " and " + std::to_string(*rightLength) + ")"));

        return RuleResult::success(isArray(l) && l.dims.empty() ? r : isArray(l) ? l : typeOf(*l.info));
    }

    RuleResult OperatorRules::logicalNot(const SemanticType& operand) const
    {
        if (!isLogicalOperand(operand, m_boolean))
            return RuleResult::failure(operatorProblem("not", operand, "'not' works on boolean, std_logic and arrays of them"));

        return RuleResult::success(isArray(operand) ? operand : typeOf(*operand.info));
    }

    RuleResult OperatorRules::shift(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        if (!isOneDimensionalArray(l) || !isLogicalScalar(l.info->element, m_boolean))
            return RuleResult::failure(operatorProblem(toString(op), l, r, "shifts and rotations work on vectors (arrays of boolean or std_logic), not on single values"));

        if (!isIntegerClass(r))
            return RuleResult::failure(operatorProblem(toString(op), l, r, "the shift amount must be an integer"));

        return RuleResult::success(l);
    }

    // ---- Concatenation --------------------------------------------------------------------------

    RuleResult OperatorRules::concatenate(const SemanticType& l, const SemanticType& r, const SemanticType* expected) const
    {
        const bool leftArray = isOneDimensionalArray(l);
        const bool rightArray = isOneDimensionalArray(r);

        if (leftArray && rightArray)
        {
            if (l.info != r.info)
                return RuleResult::failure(operatorProblem("&", l, r, "only arrays of the same type can be concatenated"));
            return RuleResult::success(arrayOfLength(*l.info, sumOf(staticLength(l), staticLength(r)), ascendingDimension(l)));
        }

        if (leftArray || rightArray)
        {
            const SemanticType& array = leftArray ? l : r;
            const SemanticType& element = leftArray ? r : l;

            if (element.info != array.info->element.info)
                return RuleResult::failure(operatorProblem("&", l, r, "'" + array.info->name + "' can only be extended with '"
                                                           + array.info->element.info->name + "' values"));

            return RuleResult::success(arrayOfLength(*array.info, sumOf(staticLength(array), 1), ascendingDimension(array)));
        }

        // element & element: nothing but the context says which array type is meant.
        if (expected && isOneDimensionalArray(*expected) && expected->info->element.info == l.info && l.info == r.info)
            return RuleResult::success(arrayOfLength(*expected->info, 2, ascendingDimension(*expected)));

        return RuleResult::failure(operatorProblem("&", l, r, expected
            ? "the result would need to be a '" + describe(*expected) + "', which has a different element type"
            : "the array type of the result cannot be determined from the context"));
    }

    // ---- numeric_std vectors --------------------------------------------------------------------

    RuleResult OperatorRules::numericVector(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        const bool leftVector = isNumericVector(l);
        const bool rightVector = isNumericVector(r);
        const std::string name = toString(op);
        const bool arithmetic = op == BinaryOperator::Add || op == BinaryOperator::Sub || op == BinaryOperator::Mul
                             || op == BinaryOperator::Div || op == BinaryOperator::Mod || op == BinaryOperator::Rem;

        if (leftVector && rightVector)
        {
            if (l.info != r.info)
                return RuleResult::failure(operatorProblem(name, l, r, "unsigned and signed values cannot be mixed; convert one of them"));

            const auto a = staticLength(l), b = staticLength(r);
            if (op == BinaryOperator::Add || op == BinaryOperator::Sub)
                return RuleResult::success(arrayOfLength(*l.info, a && b ? std::optional<int64_t>(std::max(*a, *b)) : std::nullopt));
            if (op == BinaryOperator::Mul) return RuleResult::success(arrayOfLength(*l.info, sumOf(a, b)));
            if (op == BinaryOperator::Div) return RuleResult::success(arrayOfLength(*l.info, a));
            if (op == BinaryOperator::Mod || op == BinaryOperator::Rem) return RuleResult::success(arrayOfLength(*l.info, b));
            return RuleResult::failure(operatorProblem(name, l, r, "operator '" + name + "' is not defined for " + l.info->name));
        }

        const SemanticType& vector = leftVector ? l : r;
        const SemanticType& scalar = leftVector ? r : l;
        if (!isIntegerClass(scalar))
            return RuleResult::failure(operatorProblem(name, l, r, describe(vector) + " can only be combined with another " + vector.info->name
                                                       + " or with an integer"));

        if (!arithmetic)
            return RuleResult::failure(operatorProblem(name, l, r, "operator '" + name + "' is not defined for " + vector.info->name));

        return RuleResult::success(arrayOfLength(*vector.info, staticLength(vector)));
    }

    RuleResult OperatorRules::vectorComparison(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        const bool sameVectors = isNumericVector(l) && isNumericVector(r) && l.info == r.info;
        const bool withInteger = (isNumericVector(l) && isIntegerClass(r)) || (isNumericVector(r) && isIntegerClass(l));

        if (!sameVectors && !withInteger)
            return RuleResult::failure(operatorProblem(toString(op), l, r, "unsigned and signed values can only be compared with the same type or with an integer; "
                                                                 "convert the other operand"));

        return RuleResult::success(typeOf(*m_boolean));
    }

    // ---- Reductions -----------------------------------------------------------------------------

    RuleResult OperatorRules::reduction(UnaryOperator op, const SemanticType& operand) const
    {
        if (!isOneDimensionalArray(operand) || !isLogicalScalar(operand.info->element, m_boolean))
            return RuleResult::failure(operatorProblem(toString(op), operand,
                "a logical reduction works on a one-dimensional array of boolean or std_logic"));

        return RuleResult::success(typeOf(*operand.info->element.info));
    }

    // ---- VHDL-2008 matching operators -----------------------------------------------------------

    RuleResult OperatorRules::condition(const SemanticType& operand) const
    {
        if (operand.info != m_stdLogic && operand.info != m_boolean)
            return RuleResult::failure(operatorProblem("??", operand, "the condition operator takes a std_logic or a boolean"));

        return RuleResult::success(typeOf(*m_boolean));
    }

    RuleResult OperatorRules::matching(BinaryOperator op, const SemanticType& l, const SemanticType& r) const
    {
        const std::string name = toString(op);
        const SemanticType result = typeOf(*m_stdLogic);
        const bool equalityKind = op == BinaryOperator::MatchEq || op == BinaryOperator::MatchNeq;

        if (l.info == m_stdLogic && r.info == m_stdLogic)
            return RuleResult::success(result);

        if (isNumericVector(l) || isNumericVector(r))
        {
            const bool sameVectors = isNumericVector(l) && isNumericVector(r) && l.info == r.info;
            const bool withInteger = (isNumericVector(l) && isIntegerClass(r)) || (isNumericVector(r) && isIntegerClass(l));
            if (sameVectors || withInteger)
                return RuleResult::success(result);
            return RuleResult::failure(operatorProblem(name, l, r, "unsigned and signed values can only be matched with the same type or with an integer"));
        }

        if (equalityKind && isOneDimensionalArray(l) && isOneDimensionalArray(r) && l.info == r.info
            && l.info->element.info == m_stdLogic)
        {
            const auto leftLength = staticLength(l);
            const auto rightLength = staticLength(r);
            if (leftLength && rightLength && *leftLength != *rightLength)
                return RuleResult::failure(operatorProblem(name, l, r, "the operands must have the same length ("
                                                           + std::to_string(*leftLength) + " and " + std::to_string(*rightLength) + ")"));
            return RuleResult::success(result);
        }

        return RuleResult::failure(operatorProblem(name, l, r, equalityKind
            ? "matching equality works on std_logic values and on arrays of them of one type"
            : "matching ordering works on std_logic values and on unsigned/signed vectors"));
    }

} // namespace Pulse::Parser
