#include "elaborator_internal.h"

#include "checked_math.h"

#include <algorithm>
#include <cmath>

namespace Pulse::Parser
{
    namespace
    {
        std::string bareName(const std::string& name)
        {
            return !name.empty() && name.front() == '"' ? name.substr(1, name.size() - 2) : name;
        }

        uint64_t maskOf(bitWidth_t width)
        {
            return width >= 64 ? ~0ULL : ((1ULL << width) - 1);
        }

        /// The two's complement value of the low `width` bits.
        int64_t signedValue(uint64_t bits, bitWidth_t width)
        {
            if (width == 0 || width >= 64)
                return static_cast<int64_t>(bits);
            const uint64_t sign = 1ULL << (width - 1);
            bits &= maskOf(width);
            return static_cast<int64_t>((bits ^ sign) - sign);
        }

        bool isRangeOperator(const Expression& expr)
        {
            auto* binary = dynamic_cast<const BinaryOpExpr*>(&expr);
            return binary && (binary->op == BinaryOperator::To || binary->op == BinaryOperator::Downto);
        }

        /// VHDL `mod`: the result has the sign of the divisor.
        int64_t vhdlMod(int64_t a, int64_t b)
        {
            const int64_t r = a % b;
            return r != 0 && ((r < 0) != (b < 0)) ? r + b : r;
        }
    } // anonymous namespace

    // ---- Entry points ---------------------------------------------------------------------------

    StaticValue UnitElaborator::requireStatic(const Expression& expr, const std::string& what, const Layout* layout)
    {
        const auto* aggregate = dynamic_cast<const AggregateExpr*>(&expr);
        if (auto value = aggregate && layout ? evaluateAggregate(*aggregate, layout) : evaluate(expr))
            return *value;
        fail(what + " must be known at elaboration time: a constant expression of literals, generics and constants", expr);
    }

    int64_t UnitElaborator::requireInteger(const Expression& expr, const std::string& what)
    {
        const StaticValue value = requireStatic(expr, what);
        if (!value.isDiscrete())
            fail(what + " must be an integer, but it is " + describeValue(value), expr);
        return value.integer;
    }

    std::optional<StaticValue> UnitElaborator::evaluate(const Expression& expr)
    {
        const SemanticType* type = m_library.typeOf(expr);
        const TypeInfo* info = type ? type->info : nullptr;

        if (auto* n = dynamic_cast<const IntegerLiteralExpr*>(&expr))
            return StaticValue::ofInteger(n->value, info);

        if (auto* n = dynamic_cast<const DoubleLiteralExpr*>(&expr))
        {
            StaticValue value;
            value.kind = StaticValue::Kind::Real;
            value.real = n->value;
            value.type = info;
            return value;
        }

        if (auto* n = dynamic_cast<const PhysicalLiteralExpr*>(&expr))
        {
            auto magnitude = evaluate(*n->magnitude);
            if (!magnitude || !info)
                return std::nullopt;
            for (const auto& unit : info->units)
            {
                if (unit.name != n->unit)
                    continue;
                StaticValue value;
                value.kind = StaticValue::Kind::Physical;
                value.type = info;
                value.integer = magnitude->kind == StaticValue::Kind::Real
                    ? static_cast<int64_t>(std::llround(magnitude->real * static_cast<double>(unit.factor)))
                    : magnitude->integer * unit.factor;
                return value;
            }
            return std::nullopt;
        }

        if (auto* n = dynamic_cast<const CharacterLiteralExpr*>(&expr))
        {
            if (!info) return std::nullopt;
            auto found = std::find(info->literals.begin(), info->literals.end(), n->value);
            if (found == info->literals.end()) return std::nullopt;
            return StaticValue::ofEnumeration(found - info->literals.begin(), info);
        }

        if (auto* n = dynamic_cast<const StringLiteralExpr*>(&expr)) return evaluateString(*n);
        if (auto* n = dynamic_cast<const SymbolExpr*>(&expr)) return evaluateName(*n);
        if (auto* n = dynamic_cast<const QualifiedExpr*>(&expr)) return evaluate(*n->operand);
        if (auto* n = dynamic_cast<const UnaryOpExpr*>(&expr)) return evaluateUnary(*n);
        if (auto* n = dynamic_cast<const BinaryOpExpr*>(&expr)) return evaluateBinary(*n);
        if (auto* n = dynamic_cast<const FunctionCallExpr*>(&expr)) return evaluateCall(*n);
        if (auto* n = dynamic_cast<const AttributeExpr*>(&expr)) return evaluateAttribute(*n);
        if (auto* n = dynamic_cast<const AggregateExpr*>(&expr)) return evaluateAggregate(*n, nullptr);
        return std::nullopt;
    }

    // ---- Names ----------------------------------------------------------------------------------

    std::optional<StaticValue> UnitElaborator::evaluateName(const SymbolExpr& expr)
    {
        if (const ASTNode* declaration = m_library.declarationOf(expr))
        {
            auto found = m_values.find(declaration);
            return found != m_values.end() ? std::optional<StaticValue>(found->second) : std::nullopt;
        }

        if (m_library.calleeOf(expr))
            return std::nullopt;        // a call of a function without arguments

        const SemanticType* type = m_library.typeOf(expr);
        if (!type || !type->info)
            return std::nullopt;

        if (type->info->cls == TypeClass::Enumeration)
        {
            const auto& literals = type->info->literals;
            auto found = std::find(literals.begin(), literals.end(), expr.name);
            if (found != literals.end())
                return StaticValue::ofEnumeration(found - literals.begin(), type->info);
        }

        if (type->info->cls == TypeClass::Physical)
        {
            for (const auto& unit : type->info->units)
            {
                if (unit.name != expr.name)
                    continue;
                StaticValue value;
                value.kind = StaticValue::Kind::Physical;
                value.integer = unit.factor;
                value.type = type->info;
                return value;
            }
        }
        return std::nullopt;
    }

    // ---- Literals of vectors --------------------------------------------------------------------

    std::optional<StaticValue> UnitElaborator::evaluateString(const StringLiteralExpr& expr)
    {
        const SemanticType* type = m_library.typeOf(expr);
        if (!type || !isOneDimensionalArray(*type) || !isStdLogic(type->info->element.info))
            return std::nullopt;
        if (expr.value.empty() || expr.value.size() > BITWIDTH_MAX)
            return std::nullopt;

        const bitWidth_t width = static_cast<bitWidth_t>(expr.value.size());
        uint64_t value = 0, mask = 0, dontCare = 0;
        for (size_t i = 0; i < expr.value.size(); ++i)
        {
            // The leftmost character is the leftmost element, which is the most significant bit.
            const uint64_t bit = 1ULL << (width - 1 - i);
            switch (expr.value[i])
            {
                case '0': case 'L': break;
                case '1': case 'H': value |= bit; break;
                case 'Z': value |= bit; mask |= bit; break;
                case '-': mask |= bit; dontCare |= bit; break;
                default: mask |= bit; break;        // 'U', 'X', 'W'
            }
        }
        return StaticValue::ofVector(LogicVector(value, mask), width, type->info, dontCare);
    }

    std::optional<StaticValue> UnitElaborator::evaluateAggregate(const AggregateExpr& expr, const Layout* layout)
    {
        const SemanticType* type = m_library.typeOf(expr);
        if (!type || !isOneDimensionalArray(*type) || !isStdLogic(type->info->element.info))
            return std::nullopt;

        Layout bounds;
        if (layout && layout->isVector())
            bounds = *layout;
        else if (!type->dims.empty() && !type->unknownBounds)
            bounds = layoutOf(*type, nullptr, expr);
        else
            return std::nullopt;

        std::vector<std::optional<StaticValue>> elements(bounds.width);
        std::optional<StaticValue> others;
        size_t position = 0;

        for (const auto& element : expr.elements)
        {
            auto* named = dynamic_cast<const NamedAssociationExpr*>(element.get());
            if (!named)
            {
                auto value = evaluate(*element);
                if (!value || position >= bounds.width) return std::nullopt;
                const int64_t index = bounds.ascending ? bounds.left + static_cast<int64_t>(position) : bounds.left - static_cast<int64_t>(position);
                elements[static_cast<size_t>(bounds.bitOf(index))] = value;
                ++position;
                continue;
            }

            auto value = evaluate(*named->actual);
            auto* choices = dynamic_cast<const ChoiceListExpr*>(named->formal.get());
            if (!value || !choices) return std::nullopt;

            for (const auto& choice : choices->alternatives)
            {
                if (dynamic_cast<const OthersExpr*>(choice.get()))
                {
                    others = value;
                    continue;
                }

                StaticRange range;
                if (isRangeOperator(*choice))
                    range = evaluateRange(*choice, "A choice of an aggregate");
                else
                {
                    const int64_t index = requireInteger(*choice, "A choice of an aggregate");
                    range = { index, index, true };
                }

                for (int64_t i = 0; i < range.length(); ++i)
                {
                    const int64_t index = range.at(i);
                    if (!bounds.contains(index)) return std::nullopt;
                    elements[static_cast<size_t>(bounds.bitOf(index))] = value;
                }
            }
        }

        uint64_t value = 0, mask = 0, dontCare = 0;
        for (size_t bit = 0; bit < elements.size(); ++bit)
        {
            const std::optional<StaticValue>& element = elements[bit] ? elements[bit] : others;
            if (!element || element->kind != StaticValue::Kind::Enumeration) return std::nullopt;

            const LogicVector logic = logicOf(element->integer);
            value |= (logic.value & 1ULL) << bit;
            mask |= (logic.mask & 1ULL) << bit;
            if (element->type && element->integer >= 0 && static_cast<size_t>(element->integer) < element->type->literals.size()
                && element->type->literals[static_cast<size_t>(element->integer)] == "'-'")
                dontCare |= 1ULL << bit;
        }
        return StaticValue::ofVector(LogicVector(value, mask), bounds.width, type->info, dontCare);
    }

    // ---- Operators ------------------------------------------------------------------------------

    std::optional<StaticValue> UnitElaborator::evaluateUnary(const UnaryOpExpr& expr)
    {
        auto operand = evaluate(*expr.operand);
        if (!operand)
            return std::nullopt;

        if (auto callee = m_library.calleeOf(expr))
            return callee->builtin ? evaluateBuiltin(*callee, { *operand }, expr) : std::nullopt;

        StaticValue result = *operand;
        switch (expr.op)
        {
            case UnaryOperator::Plus:
            case UnaryOperator::Condition:
                return result;
            case UnaryOperator::Minus:
            case UnaryOperator::Abs:
            {
                const bool negate = expr.op == UnaryOperator::Minus || (operand->kind == StaticValue::Kind::Real ? operand->real < 0 : operand->integer < 0);
                if (!negate) return result;
                if (operand->kind == StaticValue::Kind::Real) { result.real = -result.real; return result; }
                if (operand->kind != StaticValue::Kind::Integer && operand->kind != StaticValue::Kind::Physical) return std::nullopt;
                auto negated = checkedSub(0, operand->integer);
                if (!negated) return std::nullopt;
                result.integer = *negated;
                return result;
            }
            case UnaryOperator::Not:
                if (operand->kind == StaticValue::Kind::Enumeration && isBoolean(operand->type))
                {
                    result.integer = 1 - operand->integer;
                    return result;
                }
                return std::nullopt;
            default:
                return std::nullopt;
        }
    }

    std::optional<StaticValue> UnitElaborator::evaluateBinary(const BinaryOpExpr& expr)
    {
        if (expr.op == BinaryOperator::To || expr.op == BinaryOperator::Downto)
            return std::nullopt;

        auto l = evaluate(*expr.left);
        if (!l) return std::nullopt;
        auto r = evaluate(*expr.right);
        if (!r) return std::nullopt;

        if (auto callee = m_library.calleeOf(expr))
            return callee->builtin ? evaluateBuiltin(*callee, { *l, *r }, expr) : std::nullopt;

        const SemanticType* type = m_library.typeOf(expr);
        const TypeInfo* resultType = type ? type->info : nullptr;
        const auto boolean = [&](bool value) { return StaticValue::ofEnumeration(value ? 1 : 0, m_library.predefinedType("boolean")); };

        // ---- Boolean logic
        if (l->kind == StaticValue::Kind::Enumeration && r->kind == StaticValue::Kind::Enumeration && isBoolean(l->type) && isBoolean(r->type))
        {
            const bool a = l->integer != 0, b = r->integer != 0;
            switch (expr.op)
            {
                case BinaryOperator::And:  return boolean(a && b);
                case BinaryOperator::Or:   return boolean(a || b);
                case BinaryOperator::Nand: return boolean(!(a && b));
                case BinaryOperator::Nor:  return boolean(!(a || b));
                case BinaryOperator::Xor:  return boolean(a != b);
                case BinaryOperator::Xnor: return boolean(a == b);
                default: break;
            }
        }

        // ---- Relational operators
        const bool relational = expr.op == BinaryOperator::Eq || expr.op == BinaryOperator::Neq || expr.op == BinaryOperator::Lt
                             || expr.op == BinaryOperator::Le || expr.op == BinaryOperator::Gt || expr.op == BinaryOperator::Ge;
        if (relational)
        {
            int comparison = 0;
            if (l->isDiscrete() && r->isDiscrete())
                comparison = l->integer < r->integer ? -1 : l->integer > r->integer ? 1 : 0;
            else if (l->kind == StaticValue::Kind::Real || r->kind == StaticValue::Kind::Real)
            {
                const double a = l->kind == StaticValue::Kind::Real ? l->real : static_cast<double>(l->integer);
                const double b = r->kind == StaticValue::Kind::Real ? r->real : static_cast<double>(r->integer);
                comparison = a < b ? -1 : a > b ? 1 : 0;
            }
            else if (l->kind == StaticValue::Kind::Vector && r->kind == StaticValue::Kind::Vector)
            {
                const bool equal = l->width == r->width && l->bits == r->bits;
                if (expr.op == BinaryOperator::Eq) return boolean(equal);
                if (expr.op == BinaryOperator::Neq) return boolean(!equal);
                if (l->width != r->width || !l->bits.isDefinite() || !r->bits.isDefinite()) return std::nullopt;
                comparison = l->bits.value < r->bits.value ? -1 : l->bits.value > r->bits.value ? 1 : 0;
            }
            else
                return std::nullopt;

            switch (expr.op)
            {
                case BinaryOperator::Eq:  return boolean(comparison == 0);
                case BinaryOperator::Neq: return boolean(comparison != 0);
                case BinaryOperator::Lt:  return boolean(comparison < 0);
                case BinaryOperator::Le:  return boolean(comparison <= 0);
                case BinaryOperator::Gt:  return boolean(comparison > 0);
                default:                  return boolean(comparison >= 0);
            }
        }

        // ---- Concatenation of vectors and elements
        if (expr.op == BinaryOperator::Concat)
        {
            const auto asVector = [&](const StaticValue& value) -> std::optional<StaticValue>
            {
                if (value.kind == StaticValue::Kind::Vector) return value;
                if (value.kind == StaticValue::Kind::Enumeration && isStdLogic(value.type))
                    return StaticValue::ofVector(logicOf(value.integer), 1, resultType);
                return std::nullopt;
            };
            auto a = asVector(*l), b = asVector(*r);
            if (!a || !b || a->width + b->width > BITWIDTH_MAX) return std::nullopt;
            const bitWidth_t width = a->width + b->width;
            const LogicVector bits(a->bits.value << b->width | b->bits.value, a->bits.mask << b->width | b->bits.mask);
            return StaticValue::ofVector(bits, width, resultType, a->dontCare << b->width | b->dontCare);
        }

        // ---- Arithmetic
        StaticValue result;
        result.type = resultType;

        if (l->kind == StaticValue::Kind::Real || r->kind == StaticValue::Kind::Real)
        {
            if (l->kind == StaticValue::Kind::Physical || r->kind == StaticValue::Kind::Physical)
            {
                const bool leftPhysical = l->kind == StaticValue::Kind::Physical;
                const double factor = leftPhysical ? r->real : l->real;
                const int64_t base = leftPhysical ? l->integer : r->integer;
                if (expr.op != BinaryOperator::Mul && !(expr.op == BinaryOperator::Div && leftPhysical)) return std::nullopt;
                result.kind = StaticValue::Kind::Physical;
                result.integer = static_cast<int64_t>(std::llround(expr.op == BinaryOperator::Mul ? base * factor : base / factor));
                return result;
            }

            const double a = l->kind == StaticValue::Kind::Real ? l->real : static_cast<double>(l->integer);
            const double b = r->kind == StaticValue::Kind::Real ? r->real : static_cast<double>(r->integer);
            result.kind = StaticValue::Kind::Real;
            switch (expr.op)
            {
                case BinaryOperator::Add: result.real = a + b; return result;
                case BinaryOperator::Sub: result.real = a - b; return result;
                case BinaryOperator::Mul: result.real = a * b; return result;
                case BinaryOperator::Div: if (b == 0.0) return std::nullopt; result.real = a / b; return result;
                case BinaryOperator::Pow: result.real = std::pow(a, b); return result;
                default: return std::nullopt;
            }
        }

        if (!(l->kind == StaticValue::Kind::Integer || l->kind == StaticValue::Kind::Physical)
            || !(r->kind == StaticValue::Kind::Integer || r->kind == StaticValue::Kind::Physical))
            return std::nullopt;

        const bool physical = l->kind == StaticValue::Kind::Physical || r->kind == StaticValue::Kind::Physical;
        result.kind = physical ? StaticValue::Kind::Physical : StaticValue::Kind::Integer;

        std::optional<int64_t> value;
        const int64_t a = l->integer, b = r->integer;
        switch (expr.op)
        {
            case BinaryOperator::Add: value = checkedAdd(a, b); break;
            case BinaryOperator::Sub: value = checkedSub(a, b); break;
            case BinaryOperator::Mul: value = checkedMul(a, b); break;
            case BinaryOperator::Div:
                if (b == 0) return std::nullopt;
                value = a / b;
                if (l->kind == StaticValue::Kind::Physical && r->kind == StaticValue::Kind::Physical)
                    result.kind = StaticValue::Kind::Integer;     // time / time is a number
                break;
            case BinaryOperator::Mod: if (b == 0) return std::nullopt; value = vhdlMod(a, b); break;
            case BinaryOperator::Rem: if (b == 0) return std::nullopt; value = a % b; break;
            case BinaryOperator::Pow: value = checkedPower(a, b); break;
            default: return std::nullopt;
        }
        if (!value)
            return std::nullopt;
        result.integer = *value;
        return result;
    }

    // ---- Calls, indexes and conversions ---------------------------------------------------------

    std::optional<StaticValue> UnitElaborator::evaluateCall(const FunctionCallExpr& expr)
    {
        if (auto callee = m_library.calleeOf(expr))
        {
            if (!callee->builtin)
                return std::nullopt;

            std::vector<StaticValue> arguments(callee->parameters.size());
            std::vector<bool> given(callee->parameters.size(), false);
            size_t position = 0;
            for (const auto& argument : expr.arguments)
            {
                size_t formal = position++;
                const Expression* actual = argument.get();
                if (auto* named = dynamic_cast<const NamedAssociationExpr*>(argument.get()))
                {
                    auto* name = dynamic_cast<const SymbolExpr*>(named->formal.get());
                    auto found = name ? std::find(callee->parameters.begin(), callee->parameters.end(), name->name) : callee->parameters.end();
                    if (found == callee->parameters.end()) return std::nullopt;
                    formal = static_cast<size_t>(found - callee->parameters.begin());
                    actual = named->actual.get();
                }
                if (formal >= arguments.size()) return std::nullopt;
                auto value = evaluate(*actual);
                if (!value) return std::nullopt;
                arguments[formal] = *value;
                given[formal] = true;
            }
            if (std::find(given.begin(), given.end(), false) != given.end())
                return std::nullopt;
            return evaluateBuiltin(*callee, arguments, expr);
        }

        if (expr.arguments.size() != 1)
            return std::nullopt;

        // An element or a slice of a constant vector.
        if (auto* name = dynamic_cast<const SymbolExpr*>(expr.callee.get()))
        {
            if (const ASTNode* declaration = m_library.declarationOf(*name))
            {
                auto constant = m_values.find(declaration);
                if (constant == m_values.end() || constant->second.kind != StaticValue::Kind::Vector)
                    return std::nullopt;

                const SemanticType* type = m_library.typeOf(*name);
                if (!type || type->dims.empty() || type->unknownBounds)
                    return std::nullopt;
                const Layout layout = layoutOf(*type, nullptr, *name);
                const Expression& argument = *expr.arguments.front();

                if (isRangeOperator(argument))
                {
                    const StaticRange range = evaluateRange(argument, "A slice");
                    const int64_t a = layout.bitOf(range.left), b = layout.bitOf(range.right);
                    const auto high = static_cast<uint8_t>(std::max(a, b)), low = static_cast<uint8_t>(std::min(a, b));
                    return StaticValue::ofVector(constant->second.bits.range(high, low), static_cast<bitWidth_t>(high - low + 1),
                                                 constant->second.type, (constant->second.dontCare >> low) & maskOf(high - low + 1));
                }

                auto index = evaluate(argument);
                if (!index || !index->isDiscrete() || !layout.contains(index->integer))
                    return std::nullopt;
                const char bit = constant->second.bits.bit(static_cast<uint8_t>(layout.bitOf(index->integer)));
                const TypeInfo* stdLogic = m_library.predefinedType("std_logic");
                const std::string literal = std::string("'") + bit + "'";
                auto found = std::find(stdLogic->literals.begin(), stdLogic->literals.end(), literal);
                return StaticValue::ofEnumeration(found - stdLogic->literals.begin(), stdLogic);
            }

            // A type conversion `T(x)`: the prefix names a type (analysis typed no value there).
            if (!m_library.typeOf(*name))
            {
                auto operand = evaluate(*expr.arguments.front());
                const SemanticType* target = m_library.typeOf(expr);
                if (!operand || !target || !target->info)
                    return std::nullopt;

                StaticValue result = *operand;
                result.type = target->info;
                switch (target->info->cls)
                {
                    case TypeClass::Integer:
                        if (operand->kind == StaticValue::Kind::Real)
                            result.integer = static_cast<int64_t>(std::llround(operand->real));
                        else if (operand->kind != StaticValue::Kind::Integer)
                            return std::nullopt;
                        result.kind = StaticValue::Kind::Integer;
                        return result;
                    case TypeClass::Real:
                        if (operand->kind == StaticValue::Kind::Integer)
                            result.real = static_cast<double>(operand->integer);
                        else if (operand->kind != StaticValue::Kind::Real)
                            return std::nullopt;
                        result.kind = StaticValue::Kind::Real;
                        return result;
                    case TypeClass::Array:
                        return operand->kind == StaticValue::Kind::Vector ? std::optional<StaticValue>(result) : std::nullopt;
                    default:
                        return std::nullopt;
                }
            }
        }
        return std::nullopt;
    }

    std::optional<StaticValue> UnitElaborator::evaluateAttribute(const AttributeExpr& expr)
    {
        const std::string& name = expr.attributeName;
        const bool bound = name == "length" || name == "left" || name == "right" || name == "high" || name == "low";
        if (!bound)
            return std::nullopt;

        auto* prefix = dynamic_cast<const SymbolExpr*>(expr.prefix.get());
        if (!prefix)
            return std::nullopt;

        const SemanticType* resultType = m_library.typeOf(expr);
        const TypeInfo* integer = resultType ? resultType->info : m_library.predefinedType("integer");
        const auto integerValue = [&](int64_t value) { return StaticValue::ofInteger(value, integer); };

        // Bounds of a vector object: a port, signal or variable, or a constant.
        std::optional<Layout> layout;
        if (const ObjectWire* object = objectOf(*prefix))
            layout = object->layout;
        else if (const SemanticType* type = m_library.typeOf(*prefix); type && isArray(*type) && !type->dims.empty() && !type->unknownBounds)
            layout = layoutOf(*type, nullptr, *prefix);

        if (layout && layout->isVector())
        {
            if (name == "length") return integerValue(layout->width);
            if (name == "left") return integerValue(layout->left);
            if (name == "right") return integerValue(layout->right);
            if (name == "high") return integerValue(layout->high());
            return integerValue(layout->low());
        }

        // A discrete type or subtype named by the prefix.
        if (!m_library.declarationOf(*prefix) && !m_library.typeOf(*prefix))
        {
            const StaticRange range = evaluateRange(*prefix, "The prefix of '" + name + "'");
            if (name == "length") return integerValue(range.length());
            if (name == "left") return integerValue(range.left);
            if (name == "right") return integerValue(range.right);
            const int64_t low = std::min(range.left, range.right), high = std::max(range.left, range.right);
            return integerValue(name == "high" ? high : low);
        }
        return std::nullopt;
    }

    // ---- Ranges ---------------------------------------------------------------------------------

    StaticRange UnitElaborator::evaluateRange(const Expression& range, const std::string& what)
    {
        if (auto* binary = dynamic_cast<const BinaryOpExpr*>(&range); binary && isRangeOperator(range))
        {
            const StaticValue left = requireStatic(*binary->left, what);
            const StaticValue right = requireStatic(*binary->right, what);
            if (!left.isDiscrete() || !right.isDiscrete())
                fail(what + " must be a discrete range", range);
            StaticRange result{ left.integer, right.integer, binary->op == BinaryOperator::To };
            result.enumeration = left.kind == StaticValue::Kind::Enumeration;
            result.type = left.type;
            return result;
        }

        if (auto* attribute = dynamic_cast<const AttributeExpr*>(&range);
            attribute && (attribute->attributeName == "range" || attribute->attributeName == "reverse_range"))
        {
            std::optional<Layout> layout;
            if (const ObjectWire* object = objectOf(*attribute->prefix))
                layout = object->layout;
            else if (const SemanticType* type = m_library.typeOf(*attribute->prefix); type && isArray(*type))
                layout = layoutOf(*type, nullptr, *attribute->prefix);

            if (layout && layout->isVector())
            {
                StaticRange result{ layout->left, layout->right, layout->ascending };
                if (attribute->attributeName == "reverse_range")
                    result = { layout->right, layout->left, !layout->ascending };
                return result;
            }
            fail(what + ": the prefix of '" + attribute->attributeName + "' must be a vector", range);
        }

        if (auto* spec = dynamic_cast<const TypeSpec*>(&range))
        {
            if (spec->range)
                return evaluateRange(*spec->range, what);
            SymbolExpr name;
            name.name = spec->typeName;
            name.source = spec->source;
            return evaluateRange(name, what);
        }

        // A discrete type or subtype named by its type mark.
        if (auto* name = dynamic_cast<const SymbolExpr*>(&range))
        {
            for (auto scope = m_typeScopes.rbegin(); scope != m_typeScopes.rend(); ++scope)
            {
                auto found = scope->find(name->name);
                if (found == scope->end())
                    continue;

                if (auto* subtype = dynamic_cast<const SubtypeDeclaration*>(found->second))
                    return evaluateRange(*subtype->baseType, what);
                if (auto* type = dynamic_cast<const TypeDeclaration*>(found->second))
                {
                    if (auto* enumeration = dynamic_cast<const EnumeratedTypeDefinition*>(type->definition.get()))
                    {
                        StaticRange result{ 0, static_cast<int64_t>(enumeration->literals.size()) - 1, true };
                        result.enumeration = true;
                        return result;
                    }
                    if (auto* numeric = dynamic_cast<const NumericTypeDefinition*>(type->definition.get()))
                        return evaluateRange(*numeric->range, what);
                }
                break;
            }

            if (const TypeInfo* predefined = m_library.predefinedType(name->name))
            {
                if (predefined->cls == TypeClass::Enumeration)
                {
                    StaticRange result{ 0, static_cast<int64_t>(predefined->literals.size()) - 1, true };
                    result.enumeration = true;
                    result.type = predefined;
                    return result;
                }
                if (predefined->cls == TypeClass::Integer && predefined->range.present && name->name == predefined->name)
                    return { predefined->range.low, predefined->range.high, true };
            }
        }

        fail(what + " must be a range known at elaboration time", range);
    }

    // ---- IEEE builtins with static arguments ----------------------------------------------------

    std::optional<StaticValue> UnitElaborator::evaluateBuiltin(const CallTarget& callee, const std::vector<StaticValue>& arguments,
                                                               const Expression& at)
    {
        const std::string name = bareName(callee.name);
        const SemanticType* type = m_library.typeOf(at);
        const TypeInfo* resultType = type ? type->info : nullptr;
        const TypeInfo* stdLogic = m_library.predefinedType("std_logic");

        const auto isSignedVector = [](const StaticValue& value) { return value.type && value.type->family == VectorFamily::Signed; };
        const auto boolean = [&](bool value) { return StaticValue::ofEnumeration(value ? 1 : 0, m_library.predefinedType("boolean")); };
        const auto logicPosition = [&](LogicVector bit) -> int64_t
        {
            const std::string literal = std::string("'") + bit.bit(0) + "'";
            auto found = std::find(stdLogic->literals.begin(), stdLogic->literals.end(), literal);
            return found - stdLogic->literals.begin();
        };
        /// A std_logic element or a vector, as bits.
        const auto bitsOfOperand = [&](const StaticValue& value, bitWidth_t& width) -> std::optional<LogicVector>
        {
            if (value.kind == StaticValue::Kind::Vector) { width = value.width; return value.bits; }
            if (value.kind == StaticValue::Kind::Enumeration && isStdLogic(value.type)) { width = 1; return logicOf(value.integer); }
            return std::nullopt;
        };
        /// The value of a vector or integer operand as a number, extended to 64 bits by its own signedness.
        const auto numberOf = [&](const StaticValue& value) -> std::optional<int64_t>
        {
            if (value.kind == StaticValue::Kind::Integer) return value.integer;
            if (value.kind != StaticValue::Kind::Vector || !value.bits.isDefinite()) return std::nullopt;
            return isSignedVector(value) ? signedValue(value.bits.value, value.width)
                                         : static_cast<int64_t>(value.bits.value & maskOf(value.width));
        };

        // ---- Conversions
        if (name == "to_unsigned" || name == "to_signed")
        {
            if (arguments.size() != 2 || arguments[1].integer <= 0 || arguments[1].integer > BITWIDTH_MAX) return std::nullopt;
            const auto width = static_cast<bitWidth_t>(arguments[1].integer);
            return StaticValue::ofVector(LogicVector(static_cast<uint64_t>(arguments[0].integer)), width, resultType);
        }
        if (name == "to_integer")
        {
            auto number = numberOf(arguments.at(0));
            return number ? std::optional<StaticValue>(StaticValue::ofInteger(*number, resultType)) : std::nullopt;
        }
        if (name == "resize")
        {
            const StaticValue& vector = arguments.at(0);
            const int64_t size = arguments.at(1).integer;
            if (vector.kind != StaticValue::Kind::Vector || size <= 0 || size > BITWIDTH_MAX) return std::nullopt;
            const auto width = static_cast<bitWidth_t>(size);
            if (!isSignedVector(vector) || width >= vector.width)
            {
                // Zero extension for unsigned, sign extension for signed, truncation to the low bits.
                const bool negative = isSignedVector(vector) && vector.bits.bit(vector.width - 1) == '1';
                const uint64_t extension = negative ? maskOf(width) & ~maskOf(vector.width) : 0;
                return StaticValue::ofVector(LogicVector(vector.bits.value | extension, vector.bits.mask), width, resultType);
            }
            // numeric_std keeps the sign bit when a signed vector gets shorter.
            const LogicVector sign = vector.bits.range(vector.width - 1, vector.width - 1);
            const LogicVector low = width > 1 ? vector.bits.range(static_cast<uint8_t>(width - 2), 0) : LogicVector(0, 0);
            return StaticValue::ofVector(LogicVector(sign.value << (width - 1) | low.value, sign.mask << (width - 1) | low.mask), width, resultType);
        }

        // ---- Logical operators
        const bool logical = name == "and" || name == "or" || name == "nand" || name == "nor" || name == "xor" || name == "xnor";
        if ((logical || name == "not") && !arguments.empty())
        {
            bitWidth_t width = 0, otherWidth = 0;
            auto a = bitsOfOperand(arguments[0], width);
            if (!a) return std::nullopt;

            LogicVector result;
            if (arguments.size() == 1 && name == "not")
                result = ~*a;
            else if (arguments.size() == 1)
            {
                // Reduction to one element.
                LogicVector accumulator = a->range(0, 0);
                for (uint8_t bit = 1; bit < width; ++bit)
                {
                    const LogicVector next = a->range(bit, bit);
                    accumulator = (name == "and" || name == "nand") ? (accumulator & next)
                                : (name == "or" || name == "nor") ? (accumulator | next) : (accumulator ^ next);
                }
                if (name == "nand" || name == "nor" || name == "xnor") accumulator = ~accumulator;
                return StaticValue::ofEnumeration(logicPosition(accumulator.range(1)), stdLogic);
            }
            else
            {
                auto b = bitsOfOperand(arguments[1], otherWidth);
                if (!b || width != otherWidth) return std::nullopt;
                if (name == "and") result = *a & *b;
                else if (name == "or") result = *a | *b;
                else if (name == "xor") result = *a ^ *b;
                else if (name == "nand") result = ~(*a & *b);
                else if (name == "nor") result = ~(*a | *b);
                else result = ~(*a ^ *b);
            }

            if (arguments[0].kind == StaticValue::Kind::Enumeration)
                return StaticValue::ofEnumeration(logicPosition(result.range(1)), stdLogic);
            return StaticValue::ofVector(result, width, resultType);
        }
        if (name == "??")
        {
            const LogicVector bit = logicOf(arguments.at(0).integer);
            return boolean(bit.isDefinite() && bit.value == 1);
        }

        // ---- numeric_std arithmetic and comparisons
        if ((name == "+" || name == "-" || name == "*") && arguments.size() == 2)
        {
            const StaticValue& l = arguments[0];
            const StaticValue& r = arguments[1];
            bitWidth_t width = 0;
            if (l.kind == StaticValue::Kind::Vector && r.kind == StaticValue::Kind::Vector)
                width = name == "*" ? l.width + r.width : std::max(l.width, r.width);
            else
            {
                const bitWidth_t vectorWidth = l.kind == StaticValue::Kind::Vector ? l.width : r.width;
                width = name == "*" ? 2 * vectorWidth : vectorWidth;
            }
            if (width == 0 || width > BITWIDTH_MAX) return std::nullopt;

            auto a = numberOf(l), b = numberOf(r);
            if (!a || !b)
                return StaticValue::ofVector(LogicVector::Unknown(), width, resultType);
            const uint64_t ua = static_cast<uint64_t>(*a), ub = static_cast<uint64_t>(*b);
            const uint64_t result = name == "+" ? ua + ub : name == "-" ? ua - ub : ua * ub;
            return StaticValue::ofVector(LogicVector(result), width, resultType);
        }
        if (name == "=" || name == "/=" || name == "<" || name == "<=" || name == ">" || name == ">=")
        {
            if (arguments.size() != 2) return std::nullopt;
            auto a = numberOf(arguments[0]), b = numberOf(arguments[1]);
            if (!a || !b) return boolean(name == "/=");
            if (name == "=") return boolean(*a == *b);
            if (name == "/=") return boolean(*a != *b);
            if (name == "<") return boolean(*a < *b);
            if (name == "<=") return boolean(*a <= *b);
            if (name == ">") return boolean(*a > *b);
            return boolean(*a >= *b);
        }

        // ---- Shifts and rotations
        const bool shiftLeft = name == "sll" || name == "shift_left" || name == "sla";
        const bool shiftRight = name == "srl" || name == "shift_right" || name == "sra";
        const bool rotate = name == "rol" || name == "ror" || name == "rotate_left" || name == "rotate_right";
        if ((shiftLeft || shiftRight || rotate) && arguments.size() == 2 && arguments[0].kind == StaticValue::Kind::Vector)
        {
            const StaticValue& vector = arguments[0];
            int64_t amount = arguments[1].integer;
            bool left = shiftLeft || name == "rol" || name == "rotate_left";
            if (amount < 0) { amount = -amount; left = !left; }
            if (name == "sla") return std::nullopt;

            const auto shamt = static_cast<uint8_t>(std::min<int64_t>(amount, 64));
            LogicVector result;
            if (rotate)
                result = left ? vector.bits.rol(static_cast<uint8_t>(amount % vector.width), vector.width)
                              : vector.bits.ror(static_cast<uint8_t>(amount % vector.width), vector.width);
            else if (left)
                result = vector.bits.lsl(shamt, vector.width);
            else
            {
                const bool arithmetic = isSignedVector(vector) && (name == "shift_right" || name == "sra");
                result = arithmetic ? vector.bits.asr(shamt, vector.width) : vector.bits.lsr(shamt, vector.width);
            }
            return StaticValue::ofVector(result, vector.width, resultType);
        }

        if ((name == "-" || name == "abs") && arguments.size() == 1)
        {
            const StaticValue& vector = arguments[0];
            auto number = numberOf(vector);
            if (vector.kind != StaticValue::Kind::Vector) return std::nullopt;
            if (!number) return StaticValue::ofVector(LogicVector::Unknown(), vector.width, resultType);
            const int64_t result = (name == "-" || *number < 0) ? -*number : *number;
            return StaticValue::ofVector(LogicVector(static_cast<uint64_t>(result)), vector.width, resultType);
        }

        return std::nullopt;
    }

} // namespace Pulse::Parser
