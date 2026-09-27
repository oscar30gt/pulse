#include "elaborator_internal.h"

namespace Pulse::Parser
{
    namespace
    {
        /// Bits needed to hold every value from 0 to `high`.
        bitWidth_t unsignedBits(uint64_t high)
        {
            bitWidth_t bits = 1;
            while (bits < 64 && (high >> bits) != 0)
                ++bits;
            return bits;
        }

        /// Bits needed to hold every value from `low` to `high` in two's complement.
        bitWidth_t signedBits(int64_t low, int64_t high)
        {
            bitWidth_t bits = 1;
            while (bits < 64)
            {
                const int64_t min = -(int64_t(1) << (bits - 1));
                const int64_t max = (int64_t(1) << (bits - 1)) - 1;
                if (low >= min && high <= max)
                    break;
                ++bits;
            }
            return bits;
        }
    } // anonymous namespace

    bool UnitElaborator::isStdLogic(const TypeInfo* type) const
    {
        return type && type == m_library.predefinedType("std_logic");
    }

    bool UnitElaborator::isBoolean(const TypeInfo* type) const
    {
        return type && type == m_library.predefinedType("boolean");
    }

    const SemanticType& UnitElaborator::typeOf(const Expression& expr) const
    {
        const SemanticType* type = m_library.typeOf(expr);
        if (!type || !type->valid())
            fail("The analysis gave this expression no type", expr);
        return *type;
    }

    // ---- Layouts --------------------------------------------------------------------------------

    Layout UnitElaborator::scalarLayout(const TypeInfo& type, const ASTNode& at)
    {
        // An untyped integer literal takes the representation of `integer` when it needs a wire of its own.
        if (&type == &universalIntegerInfo())
            if (const TypeInfo* integer = m_library.predefinedType("integer"))
                return scalarLayout(*integer, at);

        Layout layout;
        layout.type = &type;

        switch (type.cls)
        {
            case TypeClass::Enumeration:
                if (isStdLogic(&type))
                    layout.kind = Layout::Kind::Logic;
                else if (isBoolean(&type))
                    layout.kind = Layout::Kind::Boolean;
                else
                {
                    layout.kind = Layout::Kind::Enumeration;
                    layout.width = unsignedBits(type.literals.empty() ? 0 : type.literals.size() - 1);
                }
                return layout;

            case TypeClass::Integer:
                layout.kind = Layout::Kind::Integer;
                layout.isSigned = type.range.low < 0;
                layout.width = layout.isSigned ? signedBits(type.range.low, type.range.high)
                                               : unsignedBits(static_cast<uint64_t>(type.range.high));
                return layout;

            case TypeClass::Real:
                unsupported("Objects and values of real types such as '" + type.name + "'", at);
            case TypeClass::Physical:
                unsupported("Objects and values of physical types such as '" + type.name + "' (they can only be constants)", at);
            case TypeClass::Record:
                unsupported("Records", at);
            case TypeClass::Array:
                break;
        }
        fail("'" + type.name + "' is not a scalar type", at);
    }

    Layout UnitElaborator::vectorLayout(const TypeInfo& type, bitWidth_t width, bool isSigned) const
    {
        Layout layout;
        layout.kind = Layout::Kind::Vector;
        layout.type = &type;
        layout.width = width;
        layout.isSigned = isSigned;
        layout.left = width - 1;
        layout.right = 0;
        layout.ascending = false;
        return layout;
    }

    Layout UnitElaborator::layoutOf(const SemanticType& type, const TypeSpec* spec, const ASTNode& at)
    {
        if (!type.valid())
            fail("This object has no type", at);

        if (!isArray(type))
            return scalarLayout(*type.info, at);

        if (!isOneDimensionalArray(type) || !isStdLogic(type.info->element.info))
            unsupported("Arrays other than one-dimensional arrays of std_logic (std_logic_vector, unsigned, signed ...), such as '"
                        + describe(type) + "',", at);

        std::optional<StaticRange> bounds;
        if (!type.dims.empty() && !type.unknownBounds)
            bounds = StaticRange{ type.dims.front().left, type.dims.front().right, type.dims.front().ascending };
        else if (spec)
            bounds = arrayBounds(*spec, at);

        if (!bounds)
            fail("The bounds of '" + describe(type) + "' are not known here", at);

        const int64_t length = bounds->length();
        if (length <= 0)
            unsupported("Null arrays", at);
        if (length > BITWIDTH_MAX)
            unsupported("Vectors wider than " + std::to_string(BITWIDTH_MAX) + " elements (this one has " + std::to_string(length) + ")", at);

        Layout layout = vectorLayout(*type.info, static_cast<bitWidth_t>(length), type.info->family == VectorFamily::Signed);
        layout.left = bounds->left;
        layout.right = bounds->right;
        layout.ascending = bounds->ascending;
        return layout;
    }

    Layout UnitElaborator::layoutOfExpression(const Expression& expr, std::optional<bitWidth_t> width)
    {
        const SemanticType& type = typeOf(expr);
        if (!isArray(type))
            return scalarLayout(*type.info, expr);

        if (!type.dims.empty() && !type.unknownBounds)
            return layoutOf(type, nullptr, expr);

        if (!isOneDimensionalArray(type) || !isStdLogic(type.info->element.info))
            return layoutOf(type, nullptr, expr);   // reports the unsupported array
        if (!width)
            fail("The length of this value is not known here", expr);
        return vectorLayout(*type.info, *width, type.info->family == VectorFamily::Signed);
    }

    std::optional<StaticRange> UnitElaborator::arrayBounds(const TypeSpec& spec, const ASTNode& at)
    {
        if (!spec.args.empty())
        {
            if (spec.args.size() != 1)
                unsupported("Multi-dimensional arrays", at);
            return evaluateRange(*spec.args.front(), "The index constraint of '" + spec.typeName + "'");
        }

        // No constraint on the type mark itself: the subtype or array type it names gives it.
        for (auto scope = m_typeScopes.rbegin(); scope != m_typeScopes.rend(); ++scope)
        {
            auto found = scope->find(spec.typeName);
            if (found == scope->end())
                continue;

            if (auto* subtype = dynamic_cast<const SubtypeDeclaration*>(found->second))
                return arrayBounds(*subtype->baseType, at);
            if (auto* type = dynamic_cast<const TypeDeclaration*>(found->second))
                if (auto* array = dynamic_cast<const ArrayTypeDefinition*>(type->definition.get()); array && array->indexRanges.size() == 1)
                    return evaluateRange(*array->indexRanges.front(), "The index range of '" + type->name + "'");
            return std::nullopt;
        }
        return std::nullopt;
    }

    // ---- Values ---------------------------------------------------------------------------------

    LogicVector UnitElaborator::logicOf(int64_t position) const
    {
        const TypeInfo* stdLogic = m_library.predefinedType("std_logic");
        const std::string literal = stdLogic && position >= 0 && static_cast<size_t>(position) < stdLogic->literals.size()
            ? stdLogic->literals[static_cast<size_t>(position)] : "'X'";

        if (literal == "'0'" || literal == "'L'") return LogicVector::FromBool(false);
        if (literal == "'1'" || literal == "'H'") return LogicVector::FromBool(true);
        if (literal == "'Z'") return LogicVector::HighZ().range(1);
        return LogicVector::Unknown().range(1);     // 'U', 'X', 'W', '-'
    }

    LogicVector UnitElaborator::defaultValue(const Layout& layout, const SemanticType& type) const
    {
        switch (layout.kind)
        {
            case Layout::Kind::Logic:
            case Layout::Kind::Vector:
                return LogicVector::Unknown().range(layout.width);     // 'U' in every element
            case Layout::Kind::Boolean:
            case Layout::Kind::Enumeration:
            case Layout::Kind::Integer:
            {
                // The leftmost value of the subtype (LRM 6.4.2.3); ranges of the analysis are kept low to high.
                const ScalarRange& range = type.range.present ? type.range : type.info->range;
                const int64_t left = range.present || layout.kind == Layout::Kind::Integer ? range.low : 0;
                return LogicVector(static_cast<uint64_t>(left)).range(layout.width);
            }
        }
        return LogicVector::Unknown().range(layout.width);
    }

    LogicVector UnitElaborator::bitsOf(const StaticValue& value, const Layout& layout, const ASTNode& at) const
    {
        switch (layout.kind)
        {
            case Layout::Kind::Logic:
                if (value.kind == StaticValue::Kind::Enumeration)
                    return logicOf(value.integer);
                if (value.kind == StaticValue::Kind::Vector && value.width == 1)
                    return value.bits;
                break;

            case Layout::Kind::Boolean:
            case Layout::Kind::Enumeration:
            case Layout::Kind::Integer:
                if (value.isDiscrete())
                    return LogicVector(static_cast<uint64_t>(value.integer)).range(layout.width);
                break;

            case Layout::Kind::Vector:
                if (value.kind == StaticValue::Kind::Vector)
                {
                    if (value.width != layout.width)
                        fail("Length mismatch: the value has " + std::to_string(value.width) + " elements but "
                             + std::to_string(layout.width) + " are expected", at);
                    return value.bits;
                }
                break;
        }
        fail("The value " + describeValue(value) + " does not fit here", at);
    }

} // namespace Pulse::Parser
