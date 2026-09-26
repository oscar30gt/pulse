#include "analyzer_internal.h"

namespace Pulse::Parser
{
    namespace
    {
        /// `to`/`downto` ranges only; anything else is not an explicit range.
        const BinaryOpExpr* asExplicitRange(const Expression& expr)
        {
            auto* binary = dynamic_cast<const BinaryOpExpr*>(&expr);
            return binary && isRangeOperator(binary->op) ? binary : nullptr;
        }

        const AttributeExpr* asRangeAttribute(const Expression& expr)
        {
            auto* attribute = dynamic_cast<const AttributeExpr*>(&expr);
            return attribute && (attribute->attributeName == "range" || attribute->attributeName == "reverse_range") ? attribute : nullptr;
        }

        /// The discrete type both bounds agree on; a universal bound adopts the other bound's type.
        SemanticType commonBoundType(const SemanticType& left, const SemanticType& right, const SemanticType* context)
        {
            if (!isUniversal(left)) return left;
            if (!isUniversal(right)) return right;
            return context && isDiscrete(*context) ? withoutConstraint(*context) : left;
        }
    } // anonymous namespace

    bool AnalyzerContext::isDiscreteRange(const Expression& expr) const
    {
        if (asExplicitRange(expr) || asRangeAttribute(expr) || dynamic_cast<const TypeSpec*>(&expr))
            return true;

        return isTypeName(expr);
    }

    RangeInfo AnalyzerContext::analyzeRange(const Expression& range, const SemanticType* context)
    {
        if (auto* explicitRange = asExplicitRange(range))
            return analyzeExplicitRange(*explicitRange, context);

        if (auto attributeRange = analyzeRangeAttribute(range))
            return *attributeRange;

        if (auto typeRange = analyzeTypeRange(range))
            return *typeRange;

        fail("Expected a range such as '7 downto 0' or 'v'range'", range);
    }

    RangeInfo AnalyzerContext::analyzeExplicitRange(const BinaryOpExpr& range, const SemanticType* context)
    {
        const SemanticType left = exprType(*range.left, context);
        const SemanticType right = exprType(*range.right, isUniversal(left) ? context : &left);

        for (const SemanticType* bound : { &left, &right })
            if (!isDiscrete(*bound))
                fail("The bounds of a range must be of an integer or enumeration type, but found '" + describe(*bound) + "'", range);

        if (!isUniversal(left) && !isUniversal(right) && left.info != right.info)
            fail("The bounds of a range have different types: '" + describe(left) + "' and '" + describe(right) + "'", range);

        RangeInfo info;
        info.type = commonBoundType(left, right, context);
        info.ascending = range.op == BinaryOperator::To;

        auto leftValue = fold(*range.left, &info.type);
        auto rightValue = fold(*range.right, &info.type);
        if (leftValue && rightValue)
            info.bounds = Bounds{ leftValue->integer, rightValue->integer, info.ascending };
        else
            info.dependsOnGenerics = isStaticExpression(*range.left) && isStaticExpression(*range.right);

        return info;
    }

    std::optional<RangeInfo> AnalyzerContext::analyzeRangeAttribute(const Expression& range)
    {
        auto* attribute = asRangeAttribute(range);
        if (!attribute)
            return std::nullopt;

        const SemanticType prefix = attributePrefixType(*attribute->prefix);
        const bool reversed = attribute->attributeName == "reverse_range";
        RangeInfo info;

        if (isArray(prefix))
        {
            info.type = prefix.info->indexTypes.front();
            if (!prefix.dims.empty())
                info.bounds = prefix.dims.front();
            info.dependsOnGenerics = prefix.unknownBounds;
        }
        else if (isDiscrete(prefix) && prefix.range.present && isTypeName(*attribute->prefix))
        {
            info.type = withoutConstraint(prefix);
            info.bounds = Bounds{ prefix.range.low, prefix.range.high, true };
        }
        else
        {
            fail("'" + attribute->attributeName + " needs an array or a discrete type, but the prefix has type '" + describe(prefix) + "'", range);
        }

        info.ascending = info.bounds ? info.bounds->ascending : true;
        if (reversed && info.bounds)
        {
            info.bounds = Bounds{ info.bounds->right, info.bounds->left, !info.bounds->ascending };
            info.ascending = info.bounds->ascending;
        }
        return info;
    }

    /// A discrete type name (`natural`) or a subtype indication (`integer range 0 to 3`) used as a range: the range of that type.
    std::optional<RangeInfo> AnalyzerContext::analyzeTypeRange(const Expression& range)
    {
        SemanticType type;

        if (auto* spec = dynamic_cast<const TypeSpec*>(&range))
            type = resolveTypeSpec(*spec);
        else if (isTypeName(range))
            type = attributePrefixType(range);
        else
            return std::nullopt;

        if (!isDiscrete(type))
            fail("The type '" + describe(type) + "' cannot be used as a range; it must be an integer or enumeration type", range);

        RangeInfo info;
        info.type = withoutConstraint(type);
        info.ascending = true;

        // `natural range 7 downto 4` is a descending range; a bare type mark is ascending.
        if (auto* spec = dynamic_cast<const TypeSpec*>(&range))
            if (auto* written = spec->range ? dynamic_cast<const BinaryOpExpr*>(spec->range.get()) : nullptr)
                info.ascending = written->op != BinaryOperator::Downto;

        if (type.range.present)
            info.bounds = info.ascending ? Bounds{ type.range.low, type.range.high, true } : Bounds{ type.range.high, type.range.low, false };
        else
            info.dependsOnGenerics = true;      // a subtype whose range depends on a generic
        return info;
    }

    Bounds AnalyzerContext::requireStaticBounds(const RangeInfo& range, const Expression& node, const std::string& what)
    {
        if (!range.bounds)
            fail(what + " needs bounds that are constant at analysis time (literals, constants or attributes of constrained types)", node);

        return *range.bounds;
    }

    // ---- Index constraints ----------------------------------------------------------------------

    AnalyzerContext::IndexConstraint AnalyzerContext::analyzeIndexConstraint(const std::vector<ExpressionPtr>& ranges, const TypeInfo& arrayInfo)
    {
        if (ranges.size() != arrayInfo.indexTypes.size())
            fail("Array type '" + arrayInfo.name + "' has " + std::to_string(arrayInfo.indexTypes.size())
                 + " index range(s), but " + std::to_string(ranges.size()) + " were given", *ranges.front());

        IndexConstraint constraint;
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            const SemanticType& indexType = arrayInfo.indexTypes[i];
            RangeInfo range = analyzeRange(*ranges[i], &indexType);

            if (!range.bounds && range.dependsOnGenerics)
            {
                constraint.unknown = true;      // constant for an instance, unknown to the analysis of the design unit
                continue;
            }

            Bounds bounds = requireStaticBounds(range, *ranges[i], "An index constraint");

            if (bounds.length() == 0)
                fail("The index range '" + describeBounds(bounds) + "' is empty; did you mean '"
                     + describeBounds({ bounds.left, bounds.right, !bounds.ascending }) + "'?", *ranges[i]);

            if (indexType.range.present && bounds.length() > 0 && (bounds.low() < indexType.range.low || bounds.high() > indexType.range.high))
                fail("The index range '" + describeBounds(bounds) + "' is outside the range of index type '"
                     + describe(indexType) + "'", *ranges[i]);

            constraint.dims.push_back(bounds);
        }

        // A constraint is known for every dimension or for none.
        if (constraint.unknown)
            constraint.dims.clear();
        return constraint;
    }

} // namespace Pulse::Parser
