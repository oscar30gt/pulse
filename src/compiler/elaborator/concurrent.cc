#include "elaborator_internal.h"

namespace Pulse::Parser
{
    using namespace Engine;

    namespace
    {
        bool isRange(const Expression& expr)
        {
            if (auto* binary = dynamic_cast<const BinaryOpExpr*>(&expr))
                return binary->op == BinaryOperator::To || binary->op == BinaryOperator::Downto;
            if (auto* attribute = dynamic_cast<const AttributeExpr*>(&expr))
                return attribute->attributeName == "range" || attribute->attributeName == "reverse_range";
            return dynamic_cast<const TypeSpec*>(&expr) != nullptr;
        }

        std::string describeStatement(const Statement& statement, const std::string& kind)
        {
            return statement.label.empty()
                ? kind + " at line " + std::to_string(statement.source.line) + ", column " + std::to_string(statement.source.column)
                : kind + " '" + statement.label + "'";
        }
    } // anonymous namespace

    // ---- Targets --------------------------------------------------------------------------------

    UnitElaborator::TargetPart UnitElaborator::targetPart(const Expression& target)
    {
        TargetPart part;

        if (auto* name = dynamic_cast<const SymbolExpr*>(&target))
        {
            part.object = objectOf(*name);
            if (!part.object)
            {
                const ASTNode* declaration = m_library.declarationOf(*name);
                if (declaration && dynamic_cast<const AliasDeclaration*>(declaration))
                    unsupported("Aliases", target);
                fail("'" + name->name + "' cannot be assigned here", target);
            }
            part.high = static_cast<bitWidth_t>(part.object->layout.width - 1);
            part.low = 0;
            part.layout = part.object->layout;
            return part;
        }

        auto* call = dynamic_cast<const FunctionCallExpr*>(&target);
        if (!call || call->arguments.size() != 1 || !dynamic_cast<const SymbolExpr*>(call->callee.get()))
            unsupported("This assignment target", target);

        part.object = objectOf(*call->callee);
        if (!part.object)
            unsupported("This assignment target", target);
        const Layout& layout = part.object->layout;
        if (!layout.isVector())
            fail("Only a vector has elements to assign", target);
        part.whole = false;

        const Expression& argument = *call->arguments.front();
        if (isRange(argument))
        {
            const StaticRange range = evaluateRange(argument, "The bounds of the assigned slice");
            if (range.length() <= 0 || !layout.contains(range.left) || !layout.contains(range.right))
                fail("The assigned slice is outside the bounds of '" + part.object->name + "'", argument);
            const int64_t a = layout.bitOf(range.left), b = layout.bitOf(range.right);
            part.high = static_cast<bitWidth_t>(std::max(a, b));
            part.low = static_cast<bitWidth_t>(std::min(a, b));
            part.whole = part.high == layout.width - 1 && part.low == 0;
            part.layout = vectorLayout(*layout.type, static_cast<bitWidth_t>(range.length()), layout.isSigned);
            part.layout.left = range.left;
            part.layout.right = range.right;
            part.layout.ascending = range.ascending;
            return part;
        }

        auto index = evaluate(argument);
        if (!index)
            unsupported("Assigning an element chosen by a value that is not known at elaboration time", argument);
        if (!index->isDiscrete() || !layout.contains(index->integer))
            fail("Index " + describeValue(*index) + " is outside the bounds of '" + part.object->name + "'", argument);
        part.high = part.low = static_cast<bitWidth_t>(layout.bitOf(index->integer));
        part.whole = false;
        part.layout = scalarLayout(*layout.type->element.info, target);
        return part;
    }

    void UnitElaborator::driveTarget(const Expression& target, const std::string& wire, const std::string& driver)
    {
        const TargetPart part = targetPart(target);
        const ObjectWire& object = *part.object;
        if (object.objectClass == ObjectWire::Class::Variable)
            fail("A variable can only be assigned inside its process", target);

        recordDriver(object, driver, false, target);

        if (part.whole)
        {
            join(wire, object.wire);
            return;
        }

        // A part of a signal: this driver leaves the other elements at 'Z', so the drivers of the other parts win there.
        const bitWidth_t width = object.layout.width;
        std::string value = wire;
        bitWidth_t built = static_cast<bitWidth_t>(part.high - part.low + 1);
        if (part.low > 0)
        {
            value = concat(value, constantWire(LogicVector::HighZ(), part.low), static_cast<bitWidth_t>(built + part.low));
            built = static_cast<bitWidth_t>(built + part.low);
        }
        if (part.high < width - 1)
            value = concat(constantWire(LogicVector::HighZ(), static_cast<bitWidth_t>(width - 1 - part.high)), value, width);
        join(value, object.wire);
    }

    // ---- Assignments ----------------------------------------------------------------------------

    bool UnitElaborator::keepsValue(const Expression& value)
    {
        const Expression* current = &value;
        while (auto* conditional = dynamic_cast<const WhenElseExpr*>(current))
        {
            if (dynamic_cast<const UnaffectedExpr*>(conditional->trueValue.get()) || !conditional->falseValue)
                return true;
            current = conditional->falseValue.get();
        }
        return dynamic_cast<const UnaffectedExpr*>(current) != nullptr;
    }

    void UnitElaborator::elaborateSignalAssignment(const SignalAssignment& assignment)
    {
        if (dynamic_cast<const UnaffectedExpr*>(assignment.value.get()))
            return;

        // Without a final `else`, or with `unaffected`, the target keeps its value: that takes the process it stands for.
        if (keepsValue(*assignment.value))
        {
            elaborateEquivalentProcess(assignment);
            return;
        }

        const std::string driver = describeStatement(assignment, "the concurrent assignment");

        if (auto* aggregate = dynamic_cast<const AggregateExpr*>(assignment.target.get()))
        {
            // `(a, b) <= v`: the elements of v, from the left, to each target.
            std::vector<TargetPart> parts;
            bitWidth_t width = 0;
            for (const auto& element : aggregate->elements)
            {
                if (dynamic_cast<const NamedAssociationExpr*>(element.get()))
                    unsupported("Named associations in an aggregate target", *element);
                parts.push_back(targetPart(*element));
                width = static_cast<bitWidth_t>(width + parts.back().high - parts.back().low + 1);
            }

            const Layout layout = layoutOfExpression(*assignment.value, width);
            const std::string value = dynamic_cast<const WhenElseExpr*>(assignment.value.get())
                ? lowerConditional(*static_cast<const WhenElseExpr*>(assignment.value.get()), layout)
                : lowerTo(*assignment.value, layout);

            bitWidth_t next = width;
            for (size_t i = 0; i < parts.size(); ++i)
            {
                const auto size = static_cast<bitWidth_t>(parts[i].high - parts[i].low + 1);
                next = static_cast<bitWidth_t>(next - size);
                const std::string piece = slice(value, static_cast<bitWidth_t>(next + size - 1), next);
                driveTarget(*aggregate->elements[i], piece, driver);
            }
            return;
        }

        const Layout layout = targetPart(*assignment.target).layout;

        const std::string value = dynamic_cast<const WhenElseExpr*>(assignment.value.get())
            ? lowerConditional(*static_cast<const WhenElseExpr*>(assignment.value.get()), layout)
            : lowerTo(*assignment.value, layout);

        driveTarget(*assignment.target, value, driver);
    }

    std::string UnitElaborator::lowerConditional(const WhenElseExpr& value, const Layout& layout)
    {
        // Each value drives the result through a tri-state buffer enabled by its condition and by no earlier condition.
        const std::string out = newWire(layout.width);
        std::string earlier;
        const Expression* current = &value;

        while (auto* conditional = dynamic_cast<const WhenElseExpr*>(current))
        {
            const std::string condition = lowerCondition(*conditional->condition);
            const std::string enable = earlier.empty() ? condition : gate(BinaryOp::AND, condition, invert(earlier, 1), 1);
            addComponent("buf", std::make_unique<ControlledBufferInstance>(lowerTo(*conditional->trueValue, layout), out, enable));
            earlier = earlier.empty() ? condition : gate(BinaryOp::OR, earlier, condition, 1);
            current = conditional->falseValue.get();
        }

        addComponent("buf", std::make_unique<ControlledBufferInstance>(lowerTo(*current, layout), out, invert(earlier, 1)));
        return out;
    }

    // ---- Selected assignments -------------------------------------------------------------------

    std::string UnitElaborator::lowerChoiceMatch(const Operand& selector, const ChoiceListExpr& choices, bool matching)
    {
        const Layout& layout = selector.layout;
        const std::string wire = materialize(selector, layout, choices);
        const bool isSigned = layout.kind == Layout::Kind::Integer && layout.isSigned;
        std::string match;

        for (const auto& choice : choices.alternatives)
        {
            std::string alternative;
            if (isRange(*choice) || (dynamic_cast<const SymbolExpr*>(choice.get()) && !m_library.typeOf(*choice)))
            {
                const StaticRange range = evaluateRange(*choice, "A choice");
                const int64_t low = std::min(range.left, range.right), high = std::max(range.left, range.right);
                const std::string above = compare(CompareOp::GreaterThanEqual, isSigned, wire, constantWire(LogicVector(static_cast<uint64_t>(low)), layout.width));
                const std::string below = compare(CompareOp::LessThanEqual, isSigned, wire, constantWire(LogicVector(static_cast<uint64_t>(high)), layout.width));
                alternative = gate(BinaryOp::AND, above, below, 1);
            }
            else
            {
                const StaticValue value = requireStatic(*choice, "A choice");
                const LogicVector bits = bitsOf(value, layout, *choice);
                const uint64_t dontCare = matching && value.kind == StaticValue::Kind::Vector ? value.dontCare : 0;
                if (dontCare != 0)
                {
                    const std::string mask = constantWire(LogicVector(~dontCare), layout.width);
                    alternative = compare(CompareOp::Equals, false, gate(BinaryOp::AND, wire, mask, layout.width),
                                          constantWire(LogicVector(bits.value & ~dontCare, bits.mask & ~dontCare), layout.width));
                }
                else
                    alternative = compare(CompareOp::Equals, false, wire, constantWire(bits, layout.width));
            }
            match = match.empty() ? alternative : gate(BinaryOp::OR, match, alternative, 1);
        }
        return match;
    }

    void UnitElaborator::elaborateWithClause(const WithClause& with)
    {
        for (const auto& choice : with.choices)
            if (dynamic_cast<const UnaffectedExpr*>(choice->value.get()))
                unsupported("'unaffected' in a selected signal assignment", *choice->value);

        const Layout layout = targetPart(*with.target).layout;
        const Operand selector = lower(*with.selector);

        const std::string out = newWire(layout.width);
        std::string anyMatch;
        const Expression* othersValue = nullptr;

        for (const auto& choice : with.choices)
        {
            auto* choices = dynamic_cast<const ChoiceListExpr*>(choice->choices.get());
            if (!choices)
                fail("A selected assignment chooses with a list of choices", *choice);
            if (choices->alternatives.size() == 1 && dynamic_cast<const OthersExpr*>(choices->alternatives.front().get()))
            {
                othersValue = choice->value.get();
                continue;
            }

            const std::string match = lowerChoiceMatch(selector, *choices, with.matching);
            addComponent("buf", std::make_unique<ControlledBufferInstance>(lowerTo(*choice->value, layout), out, match));
            anyMatch = anyMatch.empty() ? match : gate(BinaryOp::OR, anyMatch, match, 1);
        }

        if (othersValue)
            addComponent("buf", std::make_unique<ControlledBufferInstance>(lowerTo(*othersValue, layout), out,
                                                                         anyMatch.empty() ? constantWire(LogicVector::FromBool(true), 1) : invert(anyMatch, 1)));

        driveTarget(*with.target, out, describeStatement(with, "the selected assignment"));
    }

} // namespace Pulse::Parser
