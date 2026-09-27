#include "analyzer_internal.h"

namespace Pulse::Parser
{
    void AnalyzerContext::declareType(const TypeDeclaration& decl)
    {
        const auto* define = m_definitions.find(*decl.definition);
        if (!define)
            fail("The analyzer has no handler for this kind of type definition", *decl.definition);

        // The definition is analyzed *before* the name is declared: a type cannot refer to itself.
        TypeInfo& info = newType(decl.name, TypeClass::Integer);
        (*define)(*decl.definition, info);

        if (m_loadingPrelude)
            applyPreludeTraits(info);

        Symbol symbol;
        symbol.kind = SymbolKind::Type;
        symbol.type = typeOf(info);
        symbol.declaresBaseType = true;
        declare(decl.name, std::move(symbol), decl);
    }

    void AnalyzerContext::declareSubtype(const SubtypeDeclaration& decl)
    {
        Symbol symbol;
        symbol.kind = SymbolKind::Type;
        symbol.type = resolveTypeSpec(*decl.baseType);
        declare(decl.name, std::move(symbol), decl);
    }

    /// Bounds of `range` for a numeric type definition. Sets `cls` to Integer or Real from the bound literals.
    ScalarRange AnalyzerContext::analyzeScalarRange(const Expression& range, TypeClass& cls, const ASTNode& node)
    {
        auto* explicitRange = dynamic_cast<const BinaryOpExpr*>(&range);
        if (!explicitRange || !isRangeOperator(explicitRange->op))
            fail("A numeric type needs a range such as 'range 0 to 255'", node);

        const SemanticType left = exprType(*explicitRange->left);
        const SemanticType right = exprType(*explicitRange->right);

        const bool integers = isIntegerClass(left) && isIntegerClass(right);
        const bool reals = isRealClass(left) && isRealClass(right);
        if (!integers && !reals)
            fail("The bounds of a numeric type must both be integers or both be reals, but found '"
                 + describe(left) + "' and '" + describe(right) + "'", range);

        auto leftValue = fold(*explicitRange->left);
        auto rightValue = fold(*explicitRange->right);
        if (!leftValue || !rightValue)
            fail("The bounds of a numeric type must be constant at analysis time", range);

        cls = reals ? TypeClass::Real : TypeClass::Integer;
        const bool ascending = explicitRange->op == BinaryOperator::To;

        ScalarRange result;
        result.present = true;
        result.low = ascending ? leftValue->integer : rightValue->integer;
        result.high = ascending ? rightValue->integer : leftValue->integer;
        result.realLow = ascending ? leftValue->asReal() : rightValue->asReal();
        result.realHigh = ascending ? rightValue->asReal() : leftValue->asReal();
        result.ascending = ascending;

        const bool empty = reals ? result.realLow > result.realHigh : result.low > result.high;
        if (empty)
            fail(std::string("The range of a numeric type cannot be empty (") + (ascending ? "'to'" : "'downto'")
                 + " needs " + (ascending ? "left <= right" : "left >= right") + ")", range);

        return result;
    }

    SemanticType AnalyzerContext::requireElementType(const TypeSpec& spec, const std::string& arrayName)
    {
        SemanticType element = resolveTypeSpec(spec);

        if (isArray(element) && !isConstrainedArray(element))
            fail("The element type of array '" + arrayName + "' must be constrained, but '" + describe(element) + "' is not", spec);

        return element;
    }

} // namespace Pulse::Parser
