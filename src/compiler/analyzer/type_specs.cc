#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    SemanticType AnalyzerContext::resolveTypeName(const std::string& name, const ASTNode& node)
    {
        const Symbol* symbol = find(name);

        if (!symbol)
            fail("Unknown type '" + name + "'", node);

        if (symbol->kind != SymbolKind::Type)
            fail("'" + name + "' is not a type", node);

        return symbol->type;
    }

    SemanticType AnalyzerContext::resolveTypeSpec(const TypeSpec& spec)
    {
        SemanticType type = resolveTypeName(spec.typeName, spec);

        if (!spec.args.empty())
            type = constrainArray(type, spec);

        if (spec.range)
            type = constrainScalar(type, spec);

        if (spec.resolution)
            applyResolution(type, *spec.resolution, spec);

        return type;
    }

    SemanticType AnalyzerContext::resolveObjectType(const TypeSpec& spec, const std::string& what, const std::string& name)
    {
        SemanticType type = resolveTypeSpec(spec);

        if (isArray(type) && !isConstrainedArray(type))
            fail(what + " '" + name + "' has the unconstrained type '" + describe(type)
                 + "'; give it an index range, e.g. " + type.info->name + "(7 downto 0)", spec);

        return type;
    }

    // ---- Constraints ----------------------------------------------------------------------------

    SemanticType AnalyzerContext::constrainArray(const SemanticType& base, const TypeSpec& spec)
    {
        if (!isArray(base))
            fail("Type '" + describe(base) + "' is not an array and cannot take an index constraint", spec);

        const size_t open = static_cast<size_t>(std::count_if(spec.args.begin(), spec.args.end(),
            [](const ExpressionPtr& arg) { return dynamic_cast<const OpenExpr*>(arg.get()) != nullptr; }));

        // `t(open)` leaves the array as unconstrained as it was.
        if (open > 0)
        {
            if (open != spec.args.size())
                fail("'open' cannot be mixed with ranges in an index constraint", spec);
            if (spec.args.size() != base.info->indexTypes.size())
                fail("Array type '" + base.info->name + "' has " + std::to_string(base.info->indexTypes.size())
                     + " index range(s), but " + std::to_string(spec.args.size()) + " were given", spec);
            return base;
        }

        if (isConstrainedArray(base))
            fail("Type '" + describe(base) + "' is already constrained", spec);

        const IndexConstraint constraint = analyzeIndexConstraint(spec.args, *base.info);

        SemanticType constrained;
        constrained.info = base.info;
        constrained.dims = constraint.dims;
        constrained.unknownBounds = constraint.unknown;
        constrained.resolved = base.resolved;
        return constrained;
    }

    SemanticType AnalyzerContext::constrainScalar(const SemanticType& base, const TypeSpec& spec)
    {
        if (!isScalar(base))
            fail("Type '" + describe(base) + "' is not a scalar type and cannot take a range constraint", spec);

        if (base.info->cls == TypeClass::Real)
            return constrainReal(base, spec);

        if (base.info->cls == TypeClass::Physical)
            return constrainPhysical(base, spec);

        RangeInfo range = analyzeRange(*spec.range, &base);

        // A range that depends on a generic is a constant of the instance; the subtype keeps no range here.
        if (!range.bounds && range.dependsOnGenerics)
        {
            SemanticType unconstrained = base;
            unconstrained.range.present = false;
            return unconstrained;
        }

        Bounds bounds = requireStaticBounds(range, *spec.range, "A range constraint");

        if (bounds.length() == 0)
            fail("The range '" + describeBounds(bounds) + "' of a subtype of '" + base.info->name + "' is empty", *spec.range);

        if (base.range.present && (bounds.low() < base.range.low || bounds.high() > base.range.high))
            fail("The range '" + describeBounds(bounds) + "' is outside the range of '" + describe(base) + "'", *spec.range);

        SemanticType constrained = base;
        constrained.range.present = true;
        constrained.range.low = bounds.low();
        constrained.range.high = bounds.high();
        return constrained;
    }

    /// `real range 0.0 to 1.0`: the bounds are real numbers rather than discrete values.
    SemanticType AnalyzerContext::constrainReal(const SemanticType& base, const TypeSpec& spec)
    {
        auto* range = dynamic_cast<const BinaryOpExpr*>(spec.range.get());
        if (!range || !isRangeOperator(range->op))
            fail("A range constraint must look like 'range 0.0 to 1.0'", *spec.range);

        double bounds[2];
        const Expression* operands[2] = { range->left.get(), range->right.get() };
        for (int i = 0; i < 2; ++i)
        {
            const SemanticType type = exprType(*operands[i], &base);
            if (!isRealClass(type))
                fail("The bounds of a range on '" + base.info->name + "' must be real numbers, but found '" + describe(type) + "'", *operands[i]);

            auto value = fold(*operands[i], &base);
            if (!value)
            {
                if (isStaticExpression(*operands[i]))
                    return base;            // depends on a generic: the range is a constant of the instance
                fail("A range constraint needs bounds that are constant at analysis time", *operands[i]);
            }
            bounds[i] = value->asReal();
        }

        const bool ascending = range->op == BinaryOperator::To;
        const double low = ascending ? bounds[0] : bounds[1];
        const double high = ascending ? bounds[1] : bounds[0];

        if (low > high)
            fail("The range of a subtype of '" + base.info->name + "' is empty", *spec.range);

        if (base.range.present && (low < base.range.realLow || high > base.range.realHigh))
            fail("The range is outside the range of '" + describe(base) + "'", *spec.range);

        SemanticType constrained = base;
        constrained.range.realLow = low;
        constrained.range.realHigh = high;
        return constrained;
    }

    /// `time range 0 ns to 10 ns`: the bounds are quantities of the type, kept as a count of its base unit.
    SemanticType AnalyzerContext::constrainPhysical(const SemanticType& base, const TypeSpec& spec)
    {
        auto* range = dynamic_cast<const BinaryOpExpr*>(spec.range.get());
        if (!range || !isRangeOperator(range->op))
            fail("A range constraint must look like 'range 0 ns to 10 ns'", *spec.range);

        int64_t bounds[2];
        const Expression* operands[2] = { range->left.get(), range->right.get() };
        for (int i = 0; i < 2; ++i)
        {
            const SemanticType type = exprType(*operands[i], &base);
            if (type.info != base.info)
                fail("The bounds of a range on '" + base.info->name + "' must be values of that type, but found '" + describe(type) + "'", *operands[i]);

            auto value = fold(*operands[i], &base);
            if (!value)
            {
                if (isStaticExpression(*operands[i]))
                    return base;            // depends on a generic: the range is a constant of the instance
                fail("A range constraint needs bounds that are constant at analysis time", *operands[i]);
            }
            bounds[i] = value->integer;
        }

        const bool ascending = range->op == BinaryOperator::To;
        const int64_t low = ascending ? bounds[0] : bounds[1];
        const int64_t high = ascending ? bounds[1] : bounds[0];

        if (low > high)
            fail("The range of a subtype of '" + base.info->name + "' is empty", *spec.range);

        if (base.range.present && (low < base.range.low || high > base.range.high))
            fail("The range is outside the range of '" + describe(base) + "'", *spec.range);

        SemanticType constrained = base;
        constrained.range.present = true;
        constrained.range.low = low;
        constrained.range.high = high;
        return constrained;
    }

} // namespace Pulse::Parser
