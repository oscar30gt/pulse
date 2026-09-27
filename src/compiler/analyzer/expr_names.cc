#include "analyzer_internal.h"

namespace Pulse::Parser
{
    bool AnalyzerContext::isRangeArgument(const Expression& arg) const
    {
        return isDiscreteRange(arg);
    }

    /// `prefix(arguments)` is an index, a slice, a function call or a type conversion; the syntax cannot tell them apart.
    SemanticType AnalyzerContext::typeOfCall(const FunctionCallExpr& expr, const SemanticType* expected)
    {
        if (auto* name = dynamic_cast<const SymbolExpr*>(expr.callee.get()))
        {
            const Symbol* symbol = find(name->name);

            if (!symbol)
                fail("'" + name->name + "' is not declared", *expr.callee);

            if (symbol->kind == SymbolKind::Type)
                return typeOfConversion(expr, symbol->type);

            if (symbol->kind == SymbolKind::Subprogram)
                return typeOfFunctionCall(name->name, expr, associationsOf(expr.arguments), expected);
        }

        const SemanticType prefix = exprType(*expr.callee);
        if (!isArray(prefix))
            fail("A value of type '" + describe(prefix) + "' cannot be indexed or sliced; only arrays can", expr);

        return typeOfIndexOrSlice(expr, prefix);
    }

    SemanticType AnalyzerContext::typeOfIndexOrSlice(const FunctionCallExpr& expr, const SemanticType& prefix)
    {
        const bool slice = expr.arguments.size() == 1 && isRangeArgument(*expr.arguments.front());
        return slice ? typeOfSlice(expr, prefix) : typeOfIndex(expr, prefix);
    }

    // ---- Indexing -------------------------------------------------------------------------------

    void AnalyzerContext::checkIndexInBounds(const Expression& index, const SemanticType& array, size_t dimension)
    {
        if (dimension >= array.dims.size())
            return;

        const SemanticType& indexType = array.info->indexTypes[dimension];
        auto value = fold(index, &indexType);
        if (!value)
            return;

        const Bounds& dim = array.dims[dimension];
        if (value->integer < dim.low() || value->integer > dim.high())
            fail("Index " + std::to_string(value->integer) + " is out of bounds for '" + describe(array)
                 + "' (valid indices are " + std::to_string(dim.low()) + " to " + std::to_string(dim.high()) + ")", index);
    }

    SemanticType AnalyzerContext::typeOfIndex(const FunctionCallExpr& expr, const SemanticType& prefix)
    {
        const size_t dimensions = prefix.info->indexTypes.size();
        if (expr.arguments.size() != dimensions)
            fail("'" + describe(prefix) + "' needs " + std::to_string(dimensions) + " index value(s), but " + std::to_string(expr.arguments.size())
                 + " were given", expr);

        for (size_t i = 0; i < dimensions; ++i)
        {
            const Expression& index = *expr.arguments[i];
            const SemanticType& indexType = prefix.info->indexTypes[i];

            const SemanticType given = exprType(index, &indexType);
            checkAssignable(indexType, given, &index, index, "the index of '" + prefix.info->name + "'");
            checkIndexInBounds(index, prefix, i);
        }

        return prefix.info->element;
    }

    // ---- Slicing --------------------------------------------------------------------------------

    SemanticType AnalyzerContext::typeOfSlice(const FunctionCallExpr& expr, const SemanticType& prefix)
    {
        if (!isOneDimensionalArray(prefix))
            fail("Slices of multi-dimensional arrays are not supported yet", expr);

        const SemanticType& indexType = prefix.info->indexTypes.front();
        const RangeInfo range = analyzeRange(*expr.arguments.front(), &indexType);

        if (!isUniversal(range.type) && range.type.info != indexType.info)
            fail("The slice bounds have type '" + describe(range.type) + "', but '" + describe(prefix) + "' is indexed by '"
                 + describe(indexType) + "'", expr);

        if (!prefix.dims.empty() && range.ascending != prefix.dims.front().ascending)
            fail(std::string("The slice direction '") + (range.ascending ? "to" : "downto") + "' does not match the direction '"
                 + (prefix.dims.front().ascending ? "to" : "downto") + "' of '" + describe(prefix) + "'", expr);

        SemanticType result;
        result.info = prefix.info;

        if (!range.bounds)
            return result;

        const Bounds& slice = *range.bounds;
        if (!prefix.dims.empty() && slice.length() > 0)
        {
            const Bounds& dim = prefix.dims.front();
            if (slice.low() < dim.low() || slice.high() > dim.high())
                fail("The slice '" + describeBounds(slice) + "' is out of bounds for '" + describe(prefix) + "'", expr);
        }

        result.dims = { slice };
        return result;
    }

    // ---- Type conversion ------------------------------------------------------------------------

    SemanticType AnalyzerContext::typeOfConversion(const FunctionCallExpr& expr, const SemanticType& target)
    {
        if (expr.arguments.size() != 1 || isRangeArgument(*expr.arguments.front()))
            fail("A conversion to '" + describe(target) + "' takes exactly one value", expr);

        const Expression& operand = *expr.arguments.front();
        if (needsContext(operand))
            fail("The operand of a conversion to '" + describe(target) + "' must have a type of its own; a literal is ambiguous here", operand);

        const SemanticType from = exprType(operand);
        if (!closelyRelated(from, target))
            fail("Cannot convert '" + describe(from) + "' to '" + describe(target) + "': the types are not closely related", expr);

        if (isArray(target))
        {
            const auto fromLength = staticLength(from);
            const auto toLength = staticLength(target);
            if (fromLength && toLength && *fromLength != *toLength)
                fail("Cannot convert '" + describe(from) + "' to '" + describe(target) + "': the lengths differ ("
                     + std::to_string(*fromLength) + " and " + std::to_string(*toLength) + ")", expr);

            SemanticType result = target;
            if (result.dims.empty())
                result.dims = from.dims;
            return result;
        }

        return target;
    }

    // ---- Qualified expressions and external names ------------------------------------------------

    SemanticType AnalyzerContext::typeOfQualified(const QualifiedExpr& expr, const SemanticType*)
    {
        auto* spec = dynamic_cast<const TypeSpec*>(expr.typeMark.get());
        if (!spec)
            fail("The prefix of a qualified expression must be a type mark", *expr.typeMark);

        SemanticType type = resolveTypeSpec(*spec);
        const SemanticType operand = exprType(*expr.operand, &type);
        checkAssignable(type, operand, expr.operand.get(), *expr.operand, "the qualified expression of '" + spec->typeName + "'");

        // `unsigned'("0101")`: an unconstrained array type takes the bounds of its operand.
        if (isArray(type) && !isConstrainedArray(type))
        {
            type.dims = operand.dims;
            type.unknownBounds = operand.unknownBounds;
        }
        return type;
    }

    SemanticType AnalyzerContext::typeOfExternalName(const ExternalNameExpr& expr, const SemanticType*)
    {
        checkObjectAccess(rootObject(expr), expr, false);
        return resolveObjectType(*expr.subtype, "External name", expr.path);
    }

} // namespace Pulse::Parser
