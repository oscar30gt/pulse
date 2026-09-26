#include "analyzer_internal.h"
#include "checked_math.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Pulse::Parser
{
    namespace
    {
        ConstValue makeInteger(const TypeInfo* type, int64_t value) { return { type, value, 0.0 }; }
        ConstValue makeReal(const TypeInfo* type, double value) { return { type, 0, value }; }

        /// The typed side of a binary operation: literals are universal and adopt the other operand's type.
        const TypeInfo* dominantType(const ConstValue& a, const ConstValue& b)
        {
            const bool aUniversal = a.type == &universalIntegerInfo() || a.type == &universalRealInfo();
            return aUniversal ? b.type : a.type;
        }

        bool isPhysical(const ConstValue& v) { return v.type && v.type->cls == TypeClass::Physical; }
    } // anonymous namespace

    // ---- Literals and names ---------------------------------------------------------------------

    /// Value of an enumeration literal once its owning type is decided by context or uniqueness.
    static std::optional<ConstValue> enumerationValue(const TypeInfo* owner, const std::string& literal)
    {
        if (!owner) return std::nullopt;
        auto pos = std::find(owner->literals.begin(), owner->literals.end(), literal) - owner->literals.begin();
        return makeInteger(owner, pos);
    }

    std::optional<ConstValue> AnalyzerContext::foldLiteral(const Expression& expr, const SemanticType* expected)
    {
        if (auto* n = dynamic_cast<const IntegerLiteralExpr*>(&expr))
            return makeInteger(&universalIntegerInfo(), n->value);

        if (auto* n = dynamic_cast<const DoubleLiteralExpr*>(&expr))
            return makeReal(&universalRealInfo(), n->value);

        if (auto* n = dynamic_cast<const CharacterLiteralExpr*>(&expr))
            return enumerationValue(pickEnumerationOwner(n->value, expected), n->value);

        return std::nullopt;
    }

    std::optional<ConstValue> AnalyzerContext::foldSymbol(const SymbolExpr& symbol, const SemanticType* expected)
    {
        if (const Symbol* found = find(symbol.name))
            return found->kind == SymbolKind::Constant ? found->value : std::nullopt;

        if (const TypeInfo* owner = pickEnumerationOwner(symbol.name, expected))
            return enumerationValue(owner, symbol.name);

        if (auto unit = findUnit(symbol.name))
            return makeInteger(unit->type, unit->factor);

        return std::nullopt;
    }

    std::optional<ConstValue> AnalyzerContext::foldPhysical(const PhysicalLiteralExpr& expr)
    {
        auto unit = findUnit(expr.unit);
        auto magnitude = fold(*expr.magnitude);
        if (!unit || !magnitude)
            return std::nullopt;

        if (!magnitude->isReal())
        {
            auto scaled = checkedMul(magnitude->integer, unit->factor);
            return scaled ? std::optional<ConstValue>(makeInteger(unit->type, *scaled)) : std::nullopt;
        }

        const double scaled = magnitude->real * static_cast<double>(unit->factor);
        if (std::fabs(scaled) >= 9.2e18 || std::fabs(scaled - std::round(scaled)) > 1e-6)
            return std::nullopt;

        return makeInteger(unit->type, static_cast<int64_t>(std::llround(scaled)));
    }

    // ---- Operators ------------------------------------------------------------------------------

    std::optional<ConstValue> AnalyzerContext::foldUnary(const UnaryOpExpr& expr, const SemanticType* expected)
    {
        if (m_resolvedCalls.count(&expr))
            return std::nullopt;

        auto operand = fold(*expr.operand, expected);
        if (!operand)
            return std::nullopt;

        if (expr.op == UnaryOperator::Plus)
            return operand;

        if (expr.op == UnaryOperator::Not)
            return operand->type == m_std.boolean ? std::optional<ConstValue>(makeInteger(operand->type, operand->integer ^ 1)) : std::nullopt;

        const bool negate = expr.op == UnaryOperator::Minus;
        if (!negate && expr.op != UnaryOperator::Abs)
            return std::nullopt;

        if (operand->isReal())
            return makeReal(operand->type, negate ? -operand->real : std::fabs(operand->real));

        if (operand->integer == kInt64Min)
            return std::nullopt;

        const bool flip = negate || operand->integer < 0;
        return makeInteger(operand->type, flip ? -operand->integer : operand->integer);
    }

    namespace
    {
        std::optional<ConstValue> foldIntegers(BinaryOperator op, const ConstValue& l, const ConstValue& r, const TypeInfo* type)
        {
            std::optional<int64_t> result;

            const bool division = op == BinaryOperator::Div || op == BinaryOperator::Mod || op == BinaryOperator::Rem;

            if (op == BinaryOperator::Add) result = checkedAdd(l.integer, r.integer);
            else if (op == BinaryOperator::Sub) result = checkedSub(l.integer, r.integer);
            else if (op == BinaryOperator::Mul) result = checkedMul(l.integer, r.integer);
            else if (op == BinaryOperator::Pow) result = checkedPower(l.integer, r.integer);
            else if (division && r.integer != 0 && !(l.integer == kInt64Min && r.integer == -1))
            {
                const int64_t remainder = l.integer % r.integer;
                if (op == BinaryOperator::Div) result = l.integer / r.integer;
                else if (op == BinaryOperator::Rem) result = remainder;
                else result = (remainder != 0 && ((remainder < 0) != (r.integer < 0))) ? remainder + r.integer : remainder;
            }

            return result ? std::optional<ConstValue>(makeInteger(type, *result)) : std::nullopt;
        }

        std::optional<ConstValue> foldReals(BinaryOperator op, const ConstValue& l, const ConstValue& r, const TypeInfo* type)
        {
            const double a = l.asReal();
            const double b = r.asReal();

            if (op == BinaryOperator::Add) return makeReal(type, a + b);
            if (op == BinaryOperator::Sub) return makeReal(type, a - b);
            if (op == BinaryOperator::Mul) return makeReal(type, a * b);
            if (op == BinaryOperator::Div && b != 0.0) return makeReal(type, a / b);
            if (op == BinaryOperator::Pow) return makeReal(type, std::pow(a, b));
            return std::nullopt;
        }

        std::optional<ConstValue> foldPhysicalOp(BinaryOperator op, const ConstValue& l, const ConstValue& r)
        {
            if (isPhysical(l) && isPhysical(r))
            {
                if (op == BinaryOperator::Div && r.integer != 0 && !(l.integer == kInt64Min && r.integer == -1))
                    return makeInteger(&universalIntegerInfo(), l.integer / r.integer);
                return (op == BinaryOperator::Add || op == BinaryOperator::Sub) ? foldIntegers(op, l, r, l.type) : std::nullopt;
            }

            const ConstValue& quantity = isPhysical(l) ? l : r;
            const ConstValue& scalar = isPhysical(l) ? r : l;
            if (op != BinaryOperator::Mul && !(op == BinaryOperator::Div && isPhysical(l)))
                return std::nullopt;

            if (scalar.isReal())
            {
                const double value = op == BinaryOperator::Mul ? quantity.integer * scalar.real : quantity.integer / scalar.real;
                return std::fabs(value) < 9.2e18 ? std::optional<ConstValue>(makeInteger(quantity.type, std::llround(value))) : std::nullopt;
            }
            return foldIntegers(op, quantity, scalar, quantity.type);
        }

        bool compareValues(BinaryOperator op, const ConstValue& l, const ConstValue& r)
        {
            const bool real = l.isReal() || r.isReal();
            const double a = l.asReal(), b = r.asReal();
            const int64_t x = l.integer, y = r.integer;

            switch (op)
            {
                case BinaryOperator::Eq:  return real ? a == b : x == y;
                case BinaryOperator::Neq: return real ? a != b : x != y;
                case BinaryOperator::Lt:  return real ? a < b : x < y;
                case BinaryOperator::Le:  return real ? a <= b : x <= y;
                case BinaryOperator::Gt:  return real ? a > b : x > y;
                default:                  return real ? a >= b : x >= y;
            }
        }
    } // anonymous namespace

    std::optional<ConstValue> AnalyzerContext::foldBinary(const BinaryOpExpr& expr, const SemanticType* expected)
    {
        if (m_resolvedCalls.count(&expr))
            return std::nullopt;        // a design-declared operator: its value is not known here

        auto left = fold(*expr.left, expected);
        auto right = fold(*expr.right, expected);
        if (!left || !right)
            return std::nullopt;

        if (isRelationalOperator(expr.op))
            return makeInteger(m_std.boolean, compareValues(expr.op, *left, *right) ? 1 : 0);

        if (left->type == m_std.boolean && right->type == m_std.boolean)
        {
            const bool a = left->integer != 0, b = right->integer != 0;
            switch (expr.op)
            {
                case BinaryOperator::And:  return makeInteger(m_std.boolean, a && b);
                case BinaryOperator::Or:   return makeInteger(m_std.boolean, a || b);
                case BinaryOperator::Xor:  return makeInteger(m_std.boolean, a != b);
                case BinaryOperator::Nand: return makeInteger(m_std.boolean, !(a && b));
                case BinaryOperator::Nor:  return makeInteger(m_std.boolean, !(a || b));
                case BinaryOperator::Xnor: return makeInteger(m_std.boolean, a == b);
                default:                   return std::nullopt;
            }
        }

        if (isPhysical(*left) || isPhysical(*right))
            return foldPhysicalOp(expr.op, *left, *right);

        const TypeInfo* type = dominantType(*left, *right);
        if (left->isReal() || right->isReal())
            return foldReals(expr.op, *left, *right, left->isReal() ? left->type : right->type);

        return foldIntegers(expr.op, *left, *right, type);
    }

    // ---- Attributes -----------------------------------------------------------------------------

    std::optional<ConstValue> AnalyzerContext::foldAttribute(const AttributeExpr& expr)
    {
        const std::string& name = expr.attributeName;
        if (name != "length" && name != "left" && name != "right" && name != "high" && name != "low")
        {
            const AttributeValue* specified = specifiedAttribute(*expr.prefix, name);
            return specified ? specified->value : std::nullopt;
        }

        const SemanticType prefix = attributePrefixType(*expr.prefix);

        if (isArray(prefix))
        {
            if (prefix.dims.empty()) return std::nullopt;
            const Bounds& dim = prefix.dims.front();
            const TypeInfo* indexInfo = prefix.info->indexTypes.front().info;

            if (name == "length") return makeInteger(&universalIntegerInfo(), dim.length());
            const int64_t value = name == "left" ? dim.left : name == "right" ? dim.right : name == "high" ? dim.high() : dim.low();
            return makeInteger(indexInfo, value);
        }

        if (!isScalar(prefix) || !prefix.range.present || name == "length")
            return std::nullopt;

        const ScalarRange& r = prefix.range;
        const bool isLow = name == "low" || name == "left";
        if (prefix.info->cls == TypeClass::Real)
            return makeReal(prefix.info, isLow ? r.realLow : r.realHigh);

        return makeInteger(prefix.info, isLow ? r.low : r.high);
    }

    // ---- Entry points ---------------------------------------------------------------------------

    std::optional<ConstValue> AnalyzerContext::fold(const Expression& expr, const SemanticType* expected)
    {
        // Folding never reports an error: an expression too deep to walk simply has no known value.
        if (m_expressionDepth >= maxExpressionDepth)
            return std::nullopt;

        DepthGuard guard(m_expressionDepth);

        if (dynamic_cast<const IntegerLiteralExpr*>(&expr) || dynamic_cast<const DoubleLiteralExpr*>(&expr)
            || dynamic_cast<const CharacterLiteralExpr*>(&expr))
            return foldLiteral(expr, expected);

        if (auto* n = dynamic_cast<const PhysicalLiteralExpr*>(&expr)) return foldPhysical(*n);
        if (auto* n = dynamic_cast<const SymbolExpr*>(&expr)) return foldSymbol(*n, expected);
        if (auto* n = dynamic_cast<const UnaryOpExpr*>(&expr)) return foldUnary(*n, expected);
        if (auto* n = dynamic_cast<const BinaryOpExpr*>(&expr)) return foldBinary(*n, expected);
        if (auto* n = dynamic_cast<const AttributeExpr*>(&expr)) return foldAttribute(*n);

        return std::nullopt;
    }

    int64_t AnalyzerContext::requireStaticInteger(const Expression& expr, const std::string& what, const SemanticType* expected)
    {
        auto value = fold(expr, expected);
        if (!value || value->isReal() || !value->type || value->type->cls == TypeClass::Real)
            fail(what + " must be a static integer expression", expr);

        return value->integer;
    }

} // namespace Pulse::Parser
