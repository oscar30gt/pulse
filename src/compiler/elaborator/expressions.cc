#include "elaborator_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    using namespace Engine;

    namespace
    {
        /// A range argument (a slice) rather than an index: `a to b`, `a downto b`, `x'range`, or a subtype indication.
        bool isRangeArgument(const Expression& argument)
        {
            if (auto* binary = dynamic_cast<const BinaryOpExpr*>(&argument))
                return binary->op == BinaryOperator::To || binary->op == BinaryOperator::Downto;
            if (auto* attribute = dynamic_cast<const AttributeExpr*>(&argument))
                return attribute->attributeName == "range" || attribute->attributeName == "reverse_range";
            return dynamic_cast<const TypeSpec*>(&argument) != nullptr;
        }
    } // anonymous namespace

    // ---- Wires and components -------------------------------------------------------------------

    std::string UnitElaborator::newWire(bitWidth_t width, const std::string& hint, LogicVector defaultValue)
    {
        const std::string name = "$" + hint + "#" + std::to_string(m_nextWire++);
        m_bp.addSignal(name, width, defaultValue.range(width));
        return name;
    }

    void UnitElaborator::addComponent(const std::string& hint, std::unique_ptr<ComponentInstance> component)
    {
        m_bp.addComponent("$" + hint + "#" + std::to_string(m_nextComponent++), std::move(component));
    }

    std::string UnitElaborator::constantWire(LogicVector bits, bitWidth_t width)
    {
        bits = bits.range(width);
        const auto key = std::make_tuple(bits.value, bits.mask, width);
        if (auto found = m_constants.find(key); found != m_constants.end())
            return found->second;

        const std::string wire = newWire(width, "k", bits);
        addComponent("const", std::make_unique<ConstantInstance>(wire, bits));
        m_constants.emplace(key, wire);
        return wire;
    }

    std::string UnitElaborator::materialize(const Operand& operand, const Layout& layout, const ASTNode& at)
    {
        if (operand.isStatic())
            return constantWire(bitsOf(*operand.value, layout, at), layout.width);

        if (operand.layout.width != layout.width)
            fail(layout.isVector() || operand.layout.isVector()
                 ? "Length mismatch: the value has " + std::to_string(operand.layout.width) + " elements but "
                   + std::to_string(layout.width) + " are expected"
                 : "The value is " + std::to_string(operand.layout.width) + " bits wide but " + std::to_string(layout.width)
                   + " bits are expected", at);
        return operand.wire;
    }

    std::string UnitElaborator::slice(const std::string& wire, bitWidth_t high, bitWidth_t low)
    {
        const std::string out = newWire(static_cast<bitWidth_t>(high - low + 1));
        addComponent("slice", std::make_unique<SplitterInstance>(wire, out, high, low));
        return out;
    }

    std::string UnitElaborator::concat(const std::string& high, const std::string& low, bitWidth_t width)
    {
        const std::string out = newWire(width);
        addComponent("concat", std::make_unique<ConcatenatorInstance>(low, high, out));
        return out;
    }

    std::string UnitElaborator::gate(BinaryOp op, const std::string& a, const std::string& b, bitWidth_t width)
    {
        const std::string out = newWire(width);
        addComponent("gate", std::make_unique<BinaryGateInstance>(a, b, out, op));
        return out;
    }

    std::string UnitElaborator::invert(const std::string& a, bitWidth_t width)
    {
        const std::string out = newWire(width);
        addComponent("not", std::make_unique<NotGateInstance>(a, out));
        return out;
    }

    std::string UnitElaborator::compare(CompareOp op, bool isSigned, const std::string& a, const std::string& b)
    {
        const std::string out = newWire(1);
        addComponent("cmp", std::make_unique<ComparatorInstance>(a, b, out, op, isSigned ? CompareMode::Signed : CompareMode::Unsigned));
        return out;
    }

    std::string UnitElaborator::shift(ShiftOp op, const std::string& in, const std::string& amount6, bitWidth_t width)
    {
        const std::string out = newWire(width);
        addComponent("shift", std::make_unique<ShifterInstance>(in, amount6, out, op));
        return out;
    }

    std::string UnitElaborator::shiftBy(ShiftOp op, const std::string& in, bitWidth_t amount, bitWidth_t width)
    {
        return shift(op, in, constantWire(LogicVector(amount), 6), width);
    }

    std::string UnitElaborator::select(const std::string& condition, const std::string& whenTrue, const std::string& whenFalse, bitWidth_t width)
    {
        const std::string out = newWire(width);
        addComponent("buf", std::make_unique<ControlledBufferInstance>(whenTrue, out, condition));
        addComponent("buf", std::make_unique<ControlledBufferInstance>(whenFalse, out, invert(condition, 1)));
        return out;
    }

    std::string UnitElaborator::resizeWire(const std::string& wire, bitWidth_t from, bitWidth_t to, bool isSigned)
    {
        if (to == from)
            return wire;
        if (to < from)
            return slice(wire, static_cast<bitWidth_t>(to - 1), 0);

        const auto extra = static_cast<bitWidth_t>(to - from);
        const std::string zeroExtended = concat(constantWire(LogicVector(0), extra), wire, to);
        if (!isSigned)
            return zeroExtended;

        // Sign extension: move the sign bit to the top, then shift it back arithmetically.
        return shiftBy(ShiftOp::ArithmeticRight, shiftBy(ShiftOp::LogicalLeft, zeroExtended, extra, to), extra, to);
    }

    std::string UnitElaborator::eventOf(const std::string& wire)
    {
        if (auto found = m_events.find(wire); found != m_events.end())
            return found->second;

        const std::string out = newWire(1, "event." + wire, LogicVector::FromBool(false));
        addComponent("event", std::make_unique<EventProbeInstance>(wire, out));
        m_events.emplace(wire, out);
        return out;
    }

    void UnitElaborator::join(const std::string& from, const std::string& to)
    {
        addComponent("join", std::make_unique<JoinInstance>(from, to));
    }

    // ---- Operands -------------------------------------------------------------------------------

    Operand UnitElaborator::wireOperand(std::string wire, Layout layout) const
    {
        Operand operand;
        operand.wire = std::move(wire);
        operand.layout = layout;
        return operand;
    }

    const ObjectWire* UnitElaborator::objectOf(const Expression& name) const
    {
        auto* symbol = dynamic_cast<const SymbolExpr*>(&name);
        const ASTNode* declaration = symbol ? m_library.declarationOf(*symbol) : nullptr;
        auto found = declaration ? m_objects.find(declaration) : m_objects.end();
        return found == m_objects.end() ? nullptr : &found->second;
    }

    void UnitElaborator::noteRead(const ObjectWire& object)
    {
        if (!m_process || object.objectClass == ObjectWire::Class::Variable)
            return;

        const auto addOnce = [&](std::vector<std::string>& list)
        {
            if (std::find(list.begin(), list.end(), object.wire) == list.end())
                list.push_back(object.wire);
        };
        addOnce(m_process->signalsRead);
        if (m_process->readSink)
            addOnce(*m_process->readSink);
    }

    // ---- Expressions ----------------------------------------------------------------------------

    Operand UnitElaborator::lower(const Expression& expr, const Layout* expected)
    {
        if (auto* aggregate = dynamic_cast<const AggregateExpr*>(&expr))
            return lowerAggregate(*aggregate, expected);

        if (auto value = evaluate(expr))
        {
            Operand operand;
            operand.value = value;
            if (expected && (value->kind != StaticValue::Kind::Vector || (expected->isVector() && expected->width == value->width)))
                operand.layout = *expected;
            else
                operand.layout = layoutOfExpression(expr, value->kind == StaticValue::Kind::Vector ? std::optional<bitWidth_t>(value->width) : std::nullopt);
            return operand;
        }

        if (auto* n = dynamic_cast<const SymbolExpr*>(&expr)) return lowerName(*n);
        if (auto* n = dynamic_cast<const FunctionCallExpr*>(&expr)) return lowerCall(*n);
        if (auto* n = dynamic_cast<const AttributeExpr*>(&expr)) return lowerAttribute(*n);
        if (auto* n = dynamic_cast<const QualifiedExpr*>(&expr)) return lower(*n->operand, expected);
        if (auto* n = dynamic_cast<const UnaryOpExpr*>(&expr)) return lowerUnary(*n, expected);
        if (auto* n = dynamic_cast<const BinaryOpExpr*>(&expr)) return lowerBinary(*n, expected);
        if (dynamic_cast<const ExternalNameExpr*>(&expr)) unsupported("External names", expr);
        if (dynamic_cast<const StringLiteralExpr*>(&expr))
            unsupported("String literals that are not vectors of std_logic of at most " + std::to_string(BITWIDTH_MAX) + " elements", expr);
        unsupported("This expression", expr);
    }

    std::string UnitElaborator::lowerTo(const Expression& expr, const Layout& layout)
    {
        return materialize(lower(expr, &layout), layout, expr);
    }

    std::string UnitElaborator::lowerCondition(const Expression& condition)
    {
        const Operand operand = lower(condition);
        if (operand.layout.kind != Layout::Kind::Boolean && operand.layout.kind != Layout::Kind::Logic)
            fail("A condition must be a boolean or a std_logic", condition);
        return materialize(operand, operand.layout, condition);
    }

    Operand UnitElaborator::lowerName(const SymbolExpr& expr)
    {
        if (const ASTNode* declaration = m_library.declarationOf(expr))
        {
            if (auto found = m_objects.find(declaration); found != m_objects.end())
            {
                noteRead(found->second);
                return wireOperand(found->second.wire, found->second.layout);
            }
            if (dynamic_cast<const AliasDeclaration*>(declaration))
                unsupported("Aliases", expr);
            if (dynamic_cast<const ParameterDeclaration*>(declaration))
                unsupported("Subprograms declared in the design", expr);
            fail("'" + expr.name + "' has no value known at elaboration time", expr);
        }

        if (m_library.calleeOf(expr))
            unsupported("Calling a function declared in the design ('" + expr.name + "')", expr);
        fail("'" + expr.name + "' cannot be used as a value here", expr);
    }

    Operand UnitElaborator::lowerCall(const FunctionCallExpr& expr)
    {
        if (auto callee = m_library.calleeOf(expr))
        {
            if (!callee->builtin)
                unsupported("Calling a function declared in the design ('" + callee->name + "')", expr);

            std::vector<const Expression*> arguments;
            for (const auto& argument : expr.arguments)
                arguments.push_back(argument.get());
            return lowerBuiltinCall(*callee, expr, arguments);
        }

        // `T(x)`: a type conversion when the prefix names a type (the analysis typed no value there).
        if (auto* name = dynamic_cast<const SymbolExpr*>(expr.callee.get()); name && !m_library.declarationOf(*name) && !m_library.typeOf(*name))
        {
            if (expr.arguments.size() != 1)
                fail("A type conversion takes one value", expr);
            return lowerConversion(expr, lower(*expr.arguments.front()));
        }

        return lowerIndexOrSlice(expr, lower(*expr.callee));
    }

    Operand UnitElaborator::lowerIndexOrSlice(const FunctionCallExpr& expr, const Operand& prefix)
    {
        if (!prefix.layout.isVector())
            fail("Only vectors can be indexed or sliced", expr);
        if (expr.arguments.size() != 1)
            unsupported("Multi-dimensional arrays", expr);

        const Expression& argument = *expr.arguments.front();
        const std::string prefixWire = materialize(prefix, prefix.layout, *expr.callee);

        if (isRangeArgument(argument))
        {
            const StaticRange range = evaluateRange(argument, "The bounds of a slice");
            if (range.length() <= 0)
                unsupported("Null slices", argument);
            if (!prefix.layout.contains(range.left) || !prefix.layout.contains(range.right))
                fail("The slice " + std::to_string(range.left) + (range.ascending ? " to " : " downto ") + std::to_string(range.right)
                     + " is outside the bounds of the vector", argument);

            const int64_t a = prefix.layout.bitOf(range.left), b = prefix.layout.bitOf(range.right);
            Layout layout = vectorLayout(*prefix.layout.type, static_cast<bitWidth_t>(range.length()), prefix.layout.isSigned);
            layout.left = range.left;
            layout.right = range.right;
            layout.ascending = range.ascending;
            return wireOperand(slice(prefixWire, static_cast<bitWidth_t>(std::max(a, b)), static_cast<bitWidth_t>(std::min(a, b))), layout);
        }

        auto index = evaluate(argument);
        if (!index)
            unsupported("Indexing a vector with a value that is not known at elaboration time", argument);
        if (!index->isDiscrete() || !prefix.layout.contains(index->integer))
            fail("Index " + describeValue(*index) + " is outside the bounds of the vector", argument);

        const auto bit = static_cast<bitWidth_t>(prefix.layout.bitOf(index->integer));
        return wireOperand(slice(prefixWire, bit, bit), scalarLayout(*prefix.layout.type->element.info, expr));
    }

    Operand UnitElaborator::lowerAttribute(const AttributeExpr& expr)
    {
        if (expr.attributeName == "event")
        {
            const ObjectWire* object = objectOf(*expr.prefix);
            if (!object || object->objectClass == ObjectWire::Class::Variable)
                unsupported("'event of something other than a whole signal or port", expr);
            noteRead(*object);
            return wireOperand(eventOf(object->wire), scalarLayout(*m_library.predefinedType("boolean"), expr));
        }
        unsupported("The attribute '" + expr.attributeName + "' here", expr);
    }

    Operand UnitElaborator::lowerAggregate(const AggregateExpr& expr, const Layout* expected)
    {
        const Layout layout = expected && expected->isVector() ? *expected : layoutOfExpression(expr);
        if (!layout.isVector())
            unsupported("Aggregates of types other than vectors of std_logic", expr);

        if (auto value = evaluateAggregate(expr, &layout))
        {
            Operand operand;
            operand.value = value;
            operand.layout = layout;
            return operand;
        }

        const Layout element = scalarLayout(*layout.type->element.info, expr);
        std::vector<std::string> bits(layout.width);
        std::string others;
        size_t position = 0;

        for (const auto& item : expr.elements)
        {
            auto* named = dynamic_cast<const NamedAssociationExpr*>(item.get());
            if (!named)
            {
                if (position >= layout.width)
                    fail("The aggregate has more elements than the vector", *item);
                const int64_t index = layout.ascending ? layout.left + static_cast<int64_t>(position) : layout.left - static_cast<int64_t>(position);
                bits[static_cast<size_t>(layout.bitOf(index))] = lowerTo(*item, element);
                ++position;
                continue;
            }

            const std::string value = lowerTo(*named->actual, element);
            auto* choices = dynamic_cast<const ChoiceListExpr*>(named->formal.get());
            if (!choices)
                fail("An aggregate element names its positions with choices", *named);

            for (const auto& choice : choices->alternatives)
            {
                if (dynamic_cast<const OthersExpr*>(choice.get()))
                {
                    others = value;
                    continue;
                }

                StaticRange range;
                if (isRangeArgument(*choice))
                    range = evaluateRange(*choice, "A choice of an aggregate");
                else
                {
                    const int64_t index = requireInteger(*choice, "A choice of an aggregate");
                    range = { index, index, true };
                }
                for (int64_t i = 0; i < range.length(); ++i)
                {
                    if (!layout.contains(range.at(i)))
                        fail("The choice " + std::to_string(range.at(i)) + " is outside the bounds of the vector", *choice);
                    bits[static_cast<size_t>(layout.bitOf(range.at(i)))] = value;
                }
            }
        }

        for (std::string& bit : bits)
        {
            if (bit.empty()) bit = others;
            if (bit.empty()) fail("The aggregate leaves an element without a value", expr);
        }

        // The most significant bit first: each step puts the next lower bit under what is built so far.
        std::string result = bits.back();
        for (size_t bit = bits.size() - 1; bit-- > 0;)
            result = concat(result, bits[bit], static_cast<bitWidth_t>(bits.size() - bit));
        return wireOperand(result, layout);
    }

    Operand UnitElaborator::lowerConversion(const FunctionCallExpr& expr, const Operand& argument)
    {
        const SemanticType& target = typeOf(expr);

        if (isArray(target))
        {
            if (!argument.layout.isVector())
                fail("Only a vector can be converted to '" + describe(target) + "'", expr);
            // Closely related vectors: the same elements, only the type (and so the signedness) changes.
            Operand converted = argument;
            converted.layout.type = target.info;
            converted.layout.isSigned = target.info->family == VectorFamily::Signed;
            if (converted.value)
                converted.value->type = target.info;
            return converted;
        }

        if (isIntegerClass(target) && argument.layout.kind == Layout::Kind::Integer)
        {
            const Layout layout = scalarLayout(*target.info, expr);
            return wireOperand(resizeWire(argument.wire, argument.layout.width, layout.width, argument.layout.isSigned), layout);
        }

        unsupported("The conversion to '" + describe(target) + "'", expr);
    }

} // namespace Pulse::Parser
