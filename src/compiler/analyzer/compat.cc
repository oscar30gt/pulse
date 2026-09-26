#include "analyzer_internal.h"

namespace Pulse::Parser
{
    namespace
    {
        /// Hint appended to a mismatch between two arrays that only differ in their type.
        std::string conversionHint(const SemanticType& target, const SemanticType& value)
        {
            const bool convertible = isOneDimensionalArray(target) && isOneDimensionalArray(value)
                                  && target.info->element.info == value.info->element.info;

            return convertible ? "; convert the value with a type conversion such as " + target.info->name + "(...)" : "";
        }

        /// A universal integer (real) literal fits every integer (real) type.
        bool universalFits(const SemanticType& target, const SemanticType& value)
        {
            return (isUniversalInteger(value) && isIntegerClass(target)) || (isUniversalReal(value) && isRealClass(target));
        }
    } // anonymous namespace

    // ---- Assignment compatibility ---------------------------------------------------------------

    std::string AnalyzerContext::assignmentProblem(const SemanticType& target, const SemanticType& value, const std::string& what) const
    {
        if (!target.valid() || !value.valid() || universalFits(target, value))
            return "";

        if (target.info != value.info)
            return "Type mismatch for " + what + ": expected '" + describe(target) + "' but the value has type '"
                 + describe(value) + "'" + conversionHint(target, value);

        const auto targetLength = staticLength(target);
        const auto valueLength = staticLength(value);
        if (targetLength && valueLength && *targetLength != *valueLength)
            return "Length mismatch for " + what + ": '" + describe(target) + "' has " + std::to_string(*targetLength)
                 + " element(s) but the value '" + describe(value) + "' has " + std::to_string(*valueLength);

        return "";
    }

    void AnalyzerContext::checkAssignable(const SemanticType& target, const SemanticType& value, const Expression* valueExpr,
                                          const ASTNode& at, const std::string& what)
    {
        if (!target.valid() || !value.valid())
            return;

        const std::string problem = assignmentProblem(target, value, what);
        if (!problem.empty())
            fail(problem, at);

        // A value known here must lie inside the range of the scalar subtype it is assigned to.
        if (valueExpr && isScalar(target))
            checkStaticRange(target, *valueExpr, what);
    }

    /// A value known at analysis time must lie inside the range of the subtype it is assigned to.
    void AnalyzerContext::checkStaticRange(const SemanticType& target, const Expression& valueExpr, const std::string& what)
    {
        if (!target.range.present)
            return;

        auto value = fold(valueExpr, &target);
        if (!value)
            return;

        const bool real = target.info->cls == TypeClass::Real;
        const bool outside = real ? (value->asReal() < target.range.realLow || value->asReal() > target.range.realHigh)
                                  : (value->integer < target.range.low || value->integer > target.range.high);
        if (!outside)
            return;

        const std::string shown = real ? std::to_string(value->asReal()) : std::to_string(value->integer);
        fail("The value " + shown + " is outside the range of '" + describe(target) + "' (" + what + ")", valueExpr);
    }

    // ---- Conditions, timeouts, discrete types ---------------------------------------------------

    /// Conditions are boolean; VHDL-2008 also accepts std_logic (the implicit `??` operator).
    void AnalyzerContext::requireCondition(const Expression& condition, const std::string& what)
    {
        const SemanticType expected = typeOf(*m_std.boolean);
        const SemanticType type = exprType(condition, &expected);

        if (type.info != m_std.boolean && type.info != m_std.stdLogic)
            fail("The condition of " + what + " must be boolean, but it has type '" + describe(type) + "'", condition);
    }

    void AnalyzerContext::requireTime(const Expression& expr, const std::string& what)
    {
        const SemanticType type = exprType(expr);

        if (type.info != m_std.time)
            fail("The " + what + " must be a value of type 'time', but it has type '" + describe(type)
                 + (isNumeric(type) ? "'; write a unit, e.g. 10 ns" : "'"), expr);

        if (auto value = fold(expr); value && value->integer < 0)
            fail("The " + what + " cannot be negative", expr);
    }

    // ---- Conversions ----------------------------------------------------------------------------

    /// Closely related types (LRM 9.3.6): numeric types with numeric types, and arrays of the same
    /// dimensionality whose elements have the same type.
    bool AnalyzerContext::closelyRelated(const SemanticType& from, const SemanticType& to) const
    {
        if (from.info == to.info)
            return true;

        const auto numeric = [](const SemanticType& t) { return isIntegerClass(t) || isRealClass(t); };
        if (numeric(from) && numeric(to))
            return true;

        if (!isArray(from) || !isArray(to) || from.info->indexTypes.size() != to.info->indexTypes.size())
            return false;

        if (from.info->element.info != to.info->element.info)
            return false;

        for (size_t i = 0; i < from.info->indexTypes.size(); ++i)
        {
            const SemanticType& a = from.info->indexTypes[i];
            const SemanticType& b = to.info->indexTypes[i];
            const bool related = (isIntegerClass(a) && isIntegerClass(b)) || a.info == b.info;
            if (!related) return false;
        }
        return true;
    }

} // namespace Pulse::Parser
