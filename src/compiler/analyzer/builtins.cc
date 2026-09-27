#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    namespace
    {
        /// The designator of a subprogram without the quotes of an operator symbol ("\"+\"" -> "+").
        std::string bareName(const std::string& name)
        {
            return !name.empty() && name.front() == '"' ? name.substr(1, name.size() - 2) : name;
        }

        bool isOneOf(const std::string& name, std::initializer_list<const char*> names)
        {
            return std::any_of(names.begin(), names.end(), [&](const char* candidate) { return name == candidate; });
        }

        /// Whether an operand of type `operand` can be the actual of a parameter of type `parameter`: the same type, or a
        /// universal literal of the parameter's class.
        bool acceptsOperand(const SemanticType& parameter, const SemanticType& operand)
        {
            if (!operand.valid() || !parameter.valid())
                return false;
            if (isUniversalInteger(operand))
                return isIntegerClass(parameter) && !isUniversal(parameter);
            if (isUniversalReal(operand))
                return isRealClass(parameter) && !isUniversal(parameter);
            return parameter.info == operand.info;
        }

        /// The static length of the operand that is an array, when the other one is not.
        std::optional<int64_t> vectorLength(const std::vector<SemanticType>& operands)
        {
            for (const SemanticType& operand : operands)
                if (isArray(operand))
                    return staticLength(operand);
            return std::nullopt;
        }

        std::optional<int64_t> sumOf(std::optional<int64_t> a, std::optional<int64_t> b)
        {
            return a && b ? std::optional<int64_t>(*a + *b) : std::nullopt;
        }
    } // anonymous namespace

    // ---- Operators ------------------------------------------------------------------------------

    std::optional<SemanticType> AnalyzerContext::typeOfBuiltinOperator(const Expression& node, const std::string& symbol,
                                                                       const std::vector<SemanticType>& operands, const SemanticType* expected)
    {
        std::vector<const SubprogramInfo*> fitting;
        for (const SubprogramInfo* overload : visibleSubprograms(symbol))
        {
            if (!overload->builtin || !overload->isFunction || overload->parameters.size() != operands.size())
                continue;

            bool accepts = true;
            for (size_t i = 0; accepts && i < operands.size(); ++i)
                accepts = acceptsOperand(overload->parameters[i].type, operands[i]);

            if (accepts)
                fitting.push_back(overload);
        }

        if (fitting.empty())
            return std::nullopt;

        if (fitting.size() > 1 && expected && expected->valid())
        {
            std::vector<const SubprogramInfo*> byResult;
            for (const SubprogramInfo* candidate : fitting)
                if (candidate->returnType.info == expected->info)
                    byResult.push_back(candidate);
            if (!byResult.empty())
                fitting = std::move(byResult);
        }

        if (fitting.size() > 1)
            fail("The operator " + symbol + " is ambiguous here; it fits " + describeOverloads(fitting), node);

        const RuleResult result = builtinOperatorResult(*fitting.front(), operands);
        if (!result.ok())
            fail(result.problem, node);

        m_resolvedCalls[&node] = fitting.front();
        return result.type;
    }

    /// Result lengths follow the IEEE packages: the logical operators keep the length of their (equally long) operands,
    /// `+`/`-` give the longer vector, `*` the sum of the lengths (twice the vector's with an integer), `/` the left length and
    /// `mod`/`rem` the right one (the vector's with an integer).
    RuleResult AnalyzerContext::builtinOperatorResult(const SubprogramInfo& op, const std::vector<SemanticType>& operands) const
    {
        const std::string name = bareName(op.name);
        const TypeInfo& result = *op.returnType.info;

        if (operands.size() == 1)
        {
            // `not v`, `-s` and `abs s` keep the operand; a reduction and `??` give their scalar result.
            if (isArray(op.returnType))
                return RuleResult::success(operands.front());
            return RuleResult::success(typeOf(result));
        }

        const SemanticType& l = operands[0];
        const SemanticType& r = operands[1];

        if (isOneOf(name, { "and", "or", "nand", "nor", "xor", "xnor" }))
        {
            if (!isArray(l))
                return RuleResult::success(typeOf(result));

            const auto leftLength = staticLength(l);
            const auto rightLength = staticLength(r);
            if (leftLength && rightLength && *leftLength != *rightLength)
                return RuleResult::failure(operatorProblem(name, l, r, "the operands must have the same length ("
                                                           + std::to_string(*leftLength) + " and " + std::to_string(*rightLength) + ")"));
            return RuleResult::success(l.dims.empty() ? r : l);
        }

        // Comparisons give a boolean; shifts and rotations keep the vector they shift.
        if (!isArray(op.returnType))
            return RuleResult::success(typeOf(result));
        if (isOneOf(name, { "sll", "srl", "sla", "sra", "rol", "ror" }))
            return RuleResult::success(l);

        const bool bothVectors = isArray(l) && isArray(r);
        const auto leftLength = isArray(l) ? staticLength(l) : std::nullopt;
        const auto rightLength = isArray(r) ? staticLength(r) : std::nullopt;

        std::optional<int64_t> length;
        if (name == "+" || name == "-")
            length = bothVectors ? (leftLength && rightLength ? std::optional<int64_t>(std::max(*leftLength, *rightLength)) : std::nullopt)
                                 : vectorLength(operands);
        else if (name == "*")
            length = bothVectors ? sumOf(leftLength, rightLength) : sumOf(vectorLength(operands), vectorLength(operands));
        else if (name == "/")
            length = isArray(l) ? leftLength : rightLength;
        else // mod, rem
            length = isArray(r) ? rightLength : leftLength;

        return RuleResult::success(arrayOfLength(result, length));
    }

    // ---- Functions ------------------------------------------------------------------------------

    /// `resize(v, n)`, `to_unsigned(x, n)` and `to_signed(x, n)` have `n` elements when `n` is static; the shifts and
    /// rotations keep the type of the vector they shift. Every other builtin gives its declared result.
    SemanticType AnalyzerContext::builtinCallResult(const SubprogramInfo& callee, const ASTNode& node, const std::vector<const Expression*>& arguments)
    {
        const std::string name = bareName(callee.name);
        const bool sized = isOneOf(name, { "resize", "to_unsigned", "to_signed" });
        const bool shifted = isOneOf(name, { "shift_left", "shift_right", "rotate_left", "rotate_right" });
        if (!sized && !shifted)
            return callee.returnType;

        const AssociationResult matched = matchAssociations(callee.parameters, arguments, AssociationKind::Call, "'" + name + "'", node);
        const auto actualOf = [&](size_t formal) -> const Expression*
        {
            for (const Binding& binding : matched.bindings)
                if (binding.formal == formal)
                    return binding.actual;
            return nullptr;
        };

        if (shifted)
        {
            const Expression* vector = actualOf(0);
            const SemanticType* type = vector ? recordedType(*vector) : nullptr;
            return type ? *type : callee.returnType;
        }

        const Expression* size = actualOf(1);
        const auto value = size ? fold(*size) : std::nullopt;
        if (!value || value->isReal() || value->integer <= 0)
            return callee.returnType;
        return arrayOfLength(*callee.returnType.info, value->integer);
    }

} // namespace Pulse::Parser
