#include "elaborator_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    using namespace Engine;

    namespace
    {
        std::string bareName(const std::string& name)
        {
            return !name.empty() && name.front() == '"' ? name.substr(1, name.size() - 2) : name;
        }

        std::optional<BinaryOp> gateOf(const std::string& name)
        {
            if (name == "and") return BinaryOp::AND;
            if (name == "or") return BinaryOp::OR;
            if (name == "xor") return BinaryOp::XOR;
            if (name == "nand") return BinaryOp::NAND;
            if (name == "nor") return BinaryOp::NOR;
            if (name == "xnor") return BinaryOp::XNOR;
            return std::nullopt;
        }

        CompareOp compareOf(BinaryOperator op)
        {
            switch (op)
            {
                case BinaryOperator::Eq: case BinaryOperator::MatchEq:   return CompareOp::Equals;
                case BinaryOperator::Neq: case BinaryOperator::MatchNeq: return CompareOp::NotEquals;
                case BinaryOperator::Lt: case BinaryOperator::MatchLt:   return CompareOp::LessThan;
                case BinaryOperator::Le: case BinaryOperator::MatchLe:   return CompareOp::LessThanEqual;
                case BinaryOperator::Gt: case BinaryOperator::MatchGt:   return CompareOp::GreaterThan;
                default:                                                  return CompareOp::GreaterThanEqual;
            }
        }

        std::optional<BinaryOperator> relationalOf(const std::string& name)
        {
            if (name == "=") return BinaryOperator::Eq;
            if (name == "/=") return BinaryOperator::Neq;
            if (name == "<") return BinaryOperator::Lt;
            if (name == "<=") return BinaryOperator::Le;
            if (name == ">") return BinaryOperator::Gt;
            if (name == ">=") return BinaryOperator::Ge;
            return std::nullopt;
        }

        std::optional<BinaryOperator> arithmeticOf(const std::string& name)
        {
            if (name == "+") return BinaryOperator::Add;
            if (name == "-") return BinaryOperator::Sub;
            if (name == "*") return BinaryOperator::Mul;
            if (name == "/") return BinaryOperator::Div;
            if (name == "mod") return BinaryOperator::Mod;
            if (name == "rem") return BinaryOperator::Rem;
            return std::nullopt;
        }

        bool isShiftName(const std::string& name)
        {
            for (const char* shift : { "sll", "srl", "sla", "sra", "rol", "ror", "shift_left", "shift_right", "rotate_left", "rotate_right" })
                if (name == shift) return true;
            return false;
        }

        /// An operand that must take its layout from its partner: an aggregate without bounds of its own.
        bool needsPartnerLayout(const Expression& expr)
        {
            return dynamic_cast<const AggregateExpr*>(&expr) != nullptr;
        }
    } // anonymous namespace

    // ---- Numeric helpers ------------------------------------------------------------------------

    std::string UnitElaborator::numericWire(const Operand& operand, bitWidth_t width, const ASTNode& at)
    {
        if (operand.isStatic())
        {
            const StaticValue& value = *operand.value;
            if (value.kind == StaticValue::Kind::Vector)
            {
                // Extended by the vector's own signedness, truncated to the low bits.
                const bool negative = value.type && value.type->family == VectorFamily::Signed && value.bits.bit(value.width - 1) == '1';
                const uint64_t extension = negative && width > value.width ? (~0ULL << value.width) : 0;
                return constantWire(LogicVector(value.bits.value | extension, value.bits.mask), width);
            }
            if (!value.isDiscrete())
                fail("A number is expected here", at);
            return constantWire(LogicVector(static_cast<uint64_t>(value.integer)), width);
        }
        return resizeWire(operand.wire, operand.layout.width, width, operand.layout.isSigned);
    }

    // ---- Unary operators ------------------------------------------------------------------------

    Operand UnitElaborator::lowerUnary(const UnaryOpExpr& expr, const Layout* expected)
    {
        const auto callee = m_library.calleeOf(expr);
        if (callee && !callee->builtin)
            unsupported("Operators declared in the design ('" + callee->name + "')", expr);

        const Operand operand = lower(*expr.operand, expr.op == UnaryOperator::Not ? expected : nullptr);
        const Layout& layout = operand.layout;

        switch (expr.op)
        {
            case UnaryOperator::Plus:
                return operand;

            case UnaryOperator::Condition:     // `??`: a std_logic condition is already a 1-bit wire
            {
                Operand condition = wireOperand(materialize(operand, layout, expr), scalarLayout(*m_library.predefinedType("boolean"), expr));
                return condition;
            }

            case UnaryOperator::Not:
                return wireOperand(invert(materialize(operand, layout, expr), layout.width), layout);

            case UnaryOperator::Minus:
            case UnaryOperator::Abs:
            {
                if (layout.kind != Layout::Kind::Integer && !(layout.isVector() && layout.isSigned))
                    fail("'" + std::string(toString(expr.op)) + "' needs a number", expr);

                const std::string value = materialize(operand, layout, expr);
                const std::string negated = newWire(layout.width);
                addComponent("sub", std::make_unique<SubtractorInstance>(constantWire(LogicVector(0), layout.width), value, negated));
                if (expr.op == UnaryOperator::Minus)
                    return wireOperand(negated, layout);

                const std::string negative = compare(CompareOp::LessThan, true, value, constantWire(LogicVector(0), layout.width));
                return wireOperand(select(negative, negated, value, layout.width), layout);
            }

            default:
                return lowerReduction(expr.op, operand, expr);
        }
    }

    Operand UnitElaborator::lowerReduction(UnaryOperator op, const Operand& operand, const Expression& at)
    {
        if (!operand.layout.isVector())
            fail("A logical reduction needs a vector", at);

        const std::string vector = materialize(operand, operand.layout, at);
        const bool negate = op == UnaryOperator::ReduceNand || op == UnaryOperator::ReduceNor || op == UnaryOperator::ReduceXnor;
        const BinaryOp combine = (op == UnaryOperator::ReduceAnd || op == UnaryOperator::ReduceNand) ? BinaryOp::AND
                               : (op == UnaryOperator::ReduceOr || op == UnaryOperator::ReduceNor) ? BinaryOp::OR : BinaryOp::XOR;

        std::string result = slice(vector, 0, 0);
        for (bitWidth_t bit = 1; bit < operand.layout.width; ++bit)
            result = gate(combine, result, slice(vector, bit, bit), 1);
        if (negate)
            result = invert(result, 1);

        return wireOperand(result, scalarLayout(*operand.layout.type->element.info, at));
    }

    // ---- Binary operators -----------------------------------------------------------------------

    Operand UnitElaborator::lowerBinary(const BinaryOpExpr& expr, const Layout* expected)
    {
        if (expr.op == BinaryOperator::To || expr.op == BinaryOperator::Downto)
            fail("A range is not a value", expr);

        const auto callee = m_library.calleeOf(expr);
        if (callee && !callee->builtin)
            unsupported("Operators declared in the design ('" + callee->name + "')", expr);

        // An aggregate operand takes the bounds of its partner.
        Operand left, right;
        if (needsPartnerLayout(*expr.left) && !needsPartnerLayout(*expr.right))
        {
            right = lower(*expr.right);
            left = lower(*expr.left, &right.layout);
        }
        else
        {
            left = lower(*expr.left, expr.op == BinaryOperator::Concat ? nullptr : expected);
            right = lower(*expr.right, needsPartnerLayout(*expr.right) ? &left.layout : nullptr);
        }

        const std::string name = callee ? bareName(callee->name) : toString(expr.op);

        if (auto gateOp = gateOf(name))
            return lowerLogical(*gateOp, false, left, right, expr);
        if (isShiftName(name))
            return lowerShift(name, left, right, expr);
        if (expr.op == BinaryOperator::Concat)
            return lowerConcatenation(left, right, expr);
        if (expr.op == BinaryOperator::MatchEq || expr.op == BinaryOperator::MatchNeq || expr.op == BinaryOperator::MatchLt
            || expr.op == BinaryOperator::MatchLe || expr.op == BinaryOperator::MatchGt || expr.op == BinaryOperator::MatchGe)
            return lowerMatching(expr.op, left, right, expr);
        if (auto relational = relationalOf(name))
            return lowerComparison(*relational, left, right, expr);
        if (auto arithmetic = arithmeticOf(name))
            return lowerArithmetic(*arithmetic, left, right, expr);

        unsupported("The operator '" + name + "' on values that are not known at elaboration time", expr);
    }

    Operand UnitElaborator::lowerLogical(BinaryOp op, bool negate, const Operand& l, const Operand& r, const Expression& at)
    {
        const Layout layout = l.isStatic() ? r.layout : l.layout;
        const std::string a = materialize(l, layout, at);
        const std::string b = materialize(r, layout, at);
        std::string result = gate(op, a, b, layout.width);
        if (negate)
            result = invert(result, layout.width);
        return wireOperand(result, layout);
    }

    Operand UnitElaborator::lowerArithmetic(BinaryOperator op, const Operand& l, const Operand& r, const Expression& at)
    {
        const bool leftVector = l.layout.isVector(), rightVector = r.layout.isVector();

        // Integer `/ mod rem **` are folded when their operands are static; the numeric_std ones are never lowered.
        if (op != BinaryOperator::Add && op != BinaryOperator::Sub && op != BinaryOperator::Mul)
        {
            if (leftVector || rightVector)
                unsupported("'" + std::string(toString(op)) + "' on unsigned and signed vectors", at);
            unsupported("'" + std::string(toString(op)) + "' on values that are not known at elaboration time", at);
        }

        // ---- Integers: the width of their type; the product keeps its low bits.
        if (!leftVector && !rightVector)
        {
            const Layout layout = l.isStatic() ? r.layout : l.layout;
            if (layout.kind != Layout::Kind::Integer)
                fail("Arithmetic needs numbers", at);

            const std::string a = materialize(l, layout, at), b = materialize(r, layout, at);
            if (op == BinaryOperator::Mul)
            {
                const std::string product = newWire(static_cast<bitWidth_t>(std::min(2 * layout.width, 64)));
                if (2 * layout.width > 64)
                    unsupported("Multiplying integers wider than 32 bits", at);
                addComponent("mul", std::make_unique<MultiplicatorInstance>(a, b, product));
                return wireOperand(slice(product, static_cast<bitWidth_t>(layout.width - 1), 0), layout);
            }

            const std::string out = newWire(layout.width);
            if (op == BinaryOperator::Add) addComponent("add", std::make_unique<AdderInstance>(a, b, out));
            else addComponent("sub", std::make_unique<SubtractorInstance>(a, b, out));
            return wireOperand(out, layout);
        }

        // ---- numeric_std: `+`/`-` give the longer vector, `*` the sum of the lengths (twice the vector's with an integer,
        // which is first converted to the vector's length).
        const Layout& vector = leftVector ? l.layout : r.layout;
        const bool isSigned = vector.isSigned;
        const bitWidth_t leftWidth = leftVector ? l.layout.width : vector.width;
        const bitWidth_t rightWidth = rightVector ? r.layout.width : vector.width;

        if (op == BinaryOperator::Mul)
        {
            const int width = leftWidth + rightWidth;
            if (width > BITWIDTH_MAX)
                unsupported("A product wider than " + std::to_string(BITWIDTH_MAX) + " bits", at);

            const Layout layout = vectorLayout(*vector.type, static_cast<bitWidth_t>(width), isSigned);
            if (!isSigned)
            {
                // The unsigned product of N and M bits is exact in N + M bits.
                const std::string out = newWire(layout.width);
                addComponent("mul", std::make_unique<MultiplicatorInstance>(numericWire(l, leftWidth, at), numericWire(r, rightWidth, at), out));
                return wireOperand(out, layout);
            }

            // Signed: extend both operands to the result width; the low half of their product is the signed product.
            if (2 * width > BITWIDTH_MAX)
                unsupported("A signed product wider than " + std::to_string(BITWIDTH_MAX / 2) + " bits", at);
            const std::string product = newWire(static_cast<bitWidth_t>(2 * width));
            addComponent("mul", std::make_unique<MultiplicatorInstance>(numericWire(l, layout.width, at), numericWire(r, layout.width, at), product));
            return wireOperand(slice(product, static_cast<bitWidth_t>(width - 1), 0), layout);
        }

        const bitWidth_t width = std::max(leftWidth, rightWidth);
        const Layout layout = vectorLayout(*vector.type, width, isSigned);
        const std::string out = newWire(width);
        const std::string a = numericWire(l, width, at), b = numericWire(r, width, at);
        if (op == BinaryOperator::Add) addComponent("add", std::make_unique<AdderInstance>(a, b, out));
        else addComponent("sub", std::make_unique<SubtractorInstance>(a, b, out));
        return wireOperand(out, layout);
    }

    Operand UnitElaborator::lowerComparison(BinaryOperator op, const Operand& l, const Operand& r, const Expression& at)
    {
        const Layout boolean = scalarLayout(*m_library.predefinedType("boolean"), at);
        const auto callee = m_library.calleeOf(at);
        const bool numeric = callee && callee->builtin;

        const auto constant = [&](bool value) { return wireOperand(constantWire(LogicVector::FromBool(value), 1), boolean); };

        if (numeric)
        {
            // numeric_std compares numbers: both operands are extended to a common width, integers as signed values.
            const bool leftVector = l.layout.isVector(), rightVector = r.layout.isVector();
            if (leftVector && rightVector)
            {
                const bitWidth_t width = std::max(l.layout.width, r.layout.width);
                const bool isSigned = l.layout.isSigned;
                return wireOperand(compare(compareOf(op), isSigned, numericWire(l, width, at), numericWire(r, width, at)), boolean);
            }

            const Operand& vector = leftVector ? l : r;
            const Operand& integer = leftVector ? r : l;
            const int integerWidth = integer.isStatic() ? 33 : integer.layout.width + (integer.layout.isSigned ? 0 : 1);
            const int width = std::max<int>(vector.layout.width + (vector.layout.isSigned ? 0 : 1), integerWidth);
            if (width > BITWIDTH_MAX)
                unsupported("Comparing a " + std::to_string(vector.layout.width) + "-bit vector with an integer", at);

            const std::string a = numericWire(l, static_cast<bitWidth_t>(width), at);
            const std::string b = numericWire(r, static_cast<bitWidth_t>(width), at);
            return wireOperand(compare(compareOf(op), true, a, b), boolean);
        }

        // Predefined comparisons: scalars of one type, or vectors element by element.
        const Layout layout = l.isStatic() ? r.layout : l.layout;
        if (l.layout.isVector() && r.layout.isVector() && l.layout.width != r.layout.width)
        {
            if (op == BinaryOperator::Eq) return constant(false);
            if (op == BinaryOperator::Neq) return constant(true);
            unsupported("Ordering vectors of different lengths", at);
        }

        const bool isSigned = layout.kind == Layout::Kind::Integer && layout.isSigned;
        return wireOperand(compare(compareOf(op), isSigned, materialize(l, layout, at), materialize(r, layout, at)), boolean);
    }

    Operand UnitElaborator::lowerMatching(BinaryOperator op, const Operand& l, const Operand& r, const Expression& at)
    {
        const Layout result = scalarLayout(*m_library.predefinedType("std_logic"), at);
        const Layout layout = l.isStatic() ? r.layout : l.layout;
        const bool equality = op == BinaryOperator::MatchEq || op == BinaryOperator::MatchNeq;

        if (!equality)
        {
            const bool isSigned = layout.isVector() && layout.isSigned;
            return wireOperand(compare(compareOf(op), isSigned, materialize(l, layout, at), materialize(r, layout, at)), result);
        }

        // '-' in a static operand matches anything: those elements are masked out of both operands.
        uint64_t dontCare = 0;
        for (const Operand* operand : { &l, &r })
            if (operand->isStatic() && operand->value->kind == StaticValue::Kind::Vector)
                dontCare |= operand->value->dontCare;

        std::string a = materialize(l, layout, at), b = materialize(r, layout, at);
        if (dontCare != 0)
        {
            const std::string mask = constantWire(LogicVector(~dontCare), layout.width);
            a = gate(BinaryOp::AND, a, mask, layout.width);
            b = gate(BinaryOp::AND, b, mask, layout.width);
        }
        return wireOperand(compare(compareOf(op), false, a, b), result);
    }

    Operand UnitElaborator::lowerShift(const std::string& name, const Operand& l, const Operand& r, const Expression& at)
    {
        if (!l.layout.isVector())
            fail("Only vectors can be shifted", at);
        if (name == "sla")
            unsupported("The operator 'sla'", at);

        const Layout layout = l.layout;
        const std::string in = materialize(l, layout, at);
        bool left = name == "sll" || name == "shift_left" || name == "rol" || name == "rotate_left";
        const bool rotate = name == "rol" || name == "ror" || name == "rotate_left" || name == "rotate_right";
        const bool arithmetic = layout.isSigned && (name == "sra" || name == "shift_right");

        const auto opOf = [&](bool toLeft)
        {
            if (rotate) return toLeft ? ShiftOp::RotateLeft : ShiftOp::RotateRight;
            if (toLeft) return ShiftOp::LogicalLeft;
            return arithmetic ? ShiftOp::ArithmeticRight : ShiftOp::LogicalRight;
        };

        if (r.isStatic())
        {
            int64_t amount = r.value->integer;
            if (amount < 0) { amount = -amount; left = !left; }
            if (rotate) amount %= layout.width;
            if (amount == 0) return wireOperand(in, layout);
            if (amount >= layout.width && !arithmetic)
                return wireOperand(constantWire(LogicVector(0), layout.width), layout);
            return wireOperand(shiftBy(opOf(left), in, static_cast<bitWidth_t>(std::min<int64_t>(amount, 63)), layout.width), layout);
        }

        // A shift amount known only at run time: its low six bits drive the shifter (a negative amount is not supported).
        const std::string amount = r.layout.width >= 6 ? slice(r.wire, 5, 0) : resizeWire(r.wire, r.layout.width, 6, false);
        return wireOperand(shift(opOf(left), in, amount, layout.width), layout);
    }

    Operand UnitElaborator::lowerConcatenation(const Operand& l, const Operand& r, const Expression& at)
    {
        const auto widthOf = [](const Operand& operand)
        {
            if (operand.isStatic() && operand.value->kind == StaticValue::Kind::Vector) return operand.value->width;
            return operand.layout.width;
        };

        const bitWidth_t leftWidth = widthOf(l), rightWidth = widthOf(r);
        if (leftWidth + rightWidth > BITWIDTH_MAX)
            unsupported("Vectors wider than " + std::to_string(BITWIDTH_MAX) + " elements", at);

        const auto wireOf = [&](const Operand& operand, bitWidth_t width)
        {
            Layout layout = operand.layout;
            layout.width = width;
            return materialize(operand, layout, at);
        };

        const bitWidth_t width = static_cast<bitWidth_t>(leftWidth + rightWidth);
        const SemanticType& type = typeOf(at);
        Layout layout = (!type.dims.empty() && !type.unknownBounds) ? layoutOf(type, nullptr, at) : vectorLayout(*type.info, width, type.info->family == VectorFamily::Signed);
        return wireOperand(concat(wireOf(l, leftWidth), wireOf(r, rightWidth), width), layout);
    }

    Operand UnitElaborator::lowerEdge(const Expression& signal, bool rising, const Expression& at)
    {
        const ObjectWire* object = objectOf(signal);
        if (!object || object->objectClass == ObjectWire::Class::Variable)
            unsupported("An edge of something other than a whole signal or port", at);
        noteRead(*object);

        const std::string level = compare(CompareOp::Equals, false, object->wire, constantWire(LogicVector::FromBool(rising), 1));
        const std::string edge = gate(BinaryOp::AND, eventOf(object->wire), level, 1);
        return wireOperand(edge, scalarLayout(*m_library.predefinedType("boolean"), at));
    }

    // ---- Calls of IEEE builtins -----------------------------------------------------------------

    Operand UnitElaborator::lowerBuiltinCall(const CallTarget& callee, const Expression& call, const std::vector<const Expression*>& arguments)
    {
        const std::string name = bareName(callee.name);

        // Arguments in parameter order, named or positional.
        std::vector<const Expression*> actuals(callee.parameters.size(), nullptr);
        size_t position = 0;
        for (const Expression* argument : arguments)
        {
            size_t formal = position++;
            const Expression* actual = argument;
            if (auto* named = dynamic_cast<const NamedAssociationExpr*>(argument))
            {
                auto* formalName = dynamic_cast<const SymbolExpr*>(named->formal.get());
                auto found = formalName ? std::find(callee.parameters.begin(), callee.parameters.end(), formalName->name) : callee.parameters.end();
                if (found == callee.parameters.end())
                    unsupported("This association in a call", *argument);
                formal = static_cast<size_t>(found - callee.parameters.begin());
                actual = named->actual.get();
            }
            if (formal >= actuals.size())
                fail("Too many arguments for '" + name + "'", *argument);
            actuals[formal] = actual;
        }
        for (const Expression* actual : actuals)
            if (!actual)
                fail("A parameter of '" + name + "' has no value", call);

        if (name == "rising_edge" || name == "falling_edge")
            return lowerEdge(*actuals[0], name == "rising_edge", call);

        if (name == "to_integer")
        {
            const Operand vector = lower(*actuals[0]);
            const Layout layout = layoutOfExpression(static_cast<const Expression&>(call));
            return wireOperand(numericWire(vector, layout.width, call), layout);
        }

        if (name == "to_unsigned" || name == "to_signed" || name == "resize")
        {
            const int64_t size = requireInteger(*actuals[1], "The size given to '" + name + "'");
            if (size <= 0 || size > BITWIDTH_MAX)
                unsupported("Vectors of " + std::to_string(size) + " elements", call);
            const auto width = static_cast<bitWidth_t>(size);
            const Operand value = lower(*actuals[0]);
            const Layout layout = layoutOfExpression(static_cast<const Expression&>(call), width);

            // numeric_std keeps the sign bit when a signed vector gets shorter.
            if (name == "resize" && value.layout.isVector() && value.layout.isSigned && width < value.layout.width && !value.isStatic())
            {
                const std::string sign = slice(value.wire, static_cast<bitWidth_t>(value.layout.width - 1), static_cast<bitWidth_t>(value.layout.width - 1));
                if (width == 1)
                    return wireOperand(sign, layout);
                return wireOperand(concat(sign, slice(value.wire, static_cast<bitWidth_t>(width - 2), 0), width), layout);
            }
            return wireOperand(numericWire(value, width, call), layout);
        }

        if (isShiftName(name))
            return lowerShift(name, lower(*actuals[0]), lower(*actuals[1]), call);

        // An operator called by its name, `"and"(a, b)`.
        if (actuals.size() == 2)
        {
            const Operand l = lower(*actuals[0]), r = lower(*actuals[1]);
            if (auto gateOp = gateOf(name)) return lowerLogical(*gateOp, false, l, r, call);
            if (auto relational = relationalOf(name)) return lowerComparison(*relational, l, r, call);
            if (auto arithmetic = arithmeticOf(name)) return lowerArithmetic(*arithmetic, l, r, call);
        }

        unsupported("The function '" + name + "' on values that are not known at elaboration time", call);
    }

} // namespace Pulse::Parser
