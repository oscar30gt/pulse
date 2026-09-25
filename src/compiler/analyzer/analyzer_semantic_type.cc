#include "semantic_type.h"

namespace Pulse::Parser
{
    // ---- Universal types ------------------------------------------------------------------------

    namespace
    {
        TypeInfo makeUniversal(const char* name, TypeClass cls)
        {
            TypeInfo info;
            info.name = name;
            info.cls = cls;
            return info;
        }

        bool sameInfo(const SemanticType& type, const TypeInfo& info) { return type.info == &info; }

        std::string describeScalarRange(const SemanticType& type)
        {
            const TypeInfo& info = *type.info;
            const ScalarRange& r = type.range;

            if (info.cls == TypeClass::Real)
                return std::to_string(r.realLow) + " to " + std::to_string(r.realHigh);

            if (info.cls == TypeClass::Enumeration && r.low >= 0 && r.high < static_cast<int64_t>(info.literals.size()))
                return info.literals[r.low] + " to " + info.literals[r.high];

            return std::to_string(r.low) + " to " + std::to_string(r.high);
        }

        bool rangeDiffersFromDeclared(const SemanticType& type)
        {
            const ScalarRange& r = type.range;
            const ScalarRange& declared = type.info->range;
            if (!r.present || !declared.present)
                return false;

            return r.low != declared.low || r.high != declared.high
                || r.realLow != declared.realLow || r.realHigh != declared.realHigh;
        }
    } // anonymous namespace

    const TypeInfo& universalIntegerInfo()
    {
        static const TypeInfo info = makeUniversal("universal integer", TypeClass::Integer);
        return info;
    }

    const TypeInfo& universalRealInfo()
    {
        static const TypeInfo info = makeUniversal("universal real", TypeClass::Real);
        return info;
    }

    namespace
    {
        SemanticType universal(const TypeInfo& info)
        {
            SemanticType type;
            type.info = &info;
            return type;
        }
    } // anonymous namespace

    SemanticType universalInteger() { return universal(universalIntegerInfo()); }
    SemanticType universalReal() { return universal(universalRealInfo()); }

    // ---- Queries --------------------------------------------------------------------------------

    bool isUniversalInteger(const SemanticType& type) { return sameInfo(type, universalIntegerInfo()); }
    bool isUniversalReal(const SemanticType& type) { return sameInfo(type, universalRealInfo()); }
    bool isUniversal(const SemanticType& type) { return isUniversalInteger(type) || isUniversalReal(type); }

    bool isIntegerClass(const SemanticType& type) { return type.info && type.info->cls == TypeClass::Integer; }
    bool isRealClass(const SemanticType& type) { return type.info && type.info->cls == TypeClass::Real; }
    bool isArray(const SemanticType& type) { return type.info && type.info->cls == TypeClass::Array; }
    bool isRecord(const SemanticType& type) { return type.info && type.info->cls == TypeClass::Record; }

    bool isNumeric(const SemanticType& type)
    {
        return isIntegerClass(type) || isRealClass(type) || (type.info && type.info->cls == TypeClass::Physical);
    }

    bool isDiscrete(const SemanticType& type)
    {
        return isIntegerClass(type) || (type.info && type.info->cls == TypeClass::Enumeration);
    }

    bool isScalar(const SemanticType& type) { return type.info && !isArray(type) && !isRecord(type); }

    bool isOneDimensionalArray(const SemanticType& type)
    {
        return isArray(type) && type.info->indexTypes.size() == 1;
    }

    SemanticType typeOf(const TypeInfo& info)
    {
        SemanticType type;
        type.info = &info;
        type.range = info.range;
        type.dims = info.declaredDims;
        type.unknownBounds = info.declaredUnknownBounds;
        return type;
    }

    SemanticType withoutConstraint(const SemanticType& type)
    {
        if (!type.info) return type;

        SemanticType result;
        result.info = type.info;
        result.range = type.info->range;
        return result;
    }

    bool isConstrainedArray(const SemanticType& type)
    {
        return isArray(type) && (!type.dims.empty() || type.unknownBounds);
    }

    std::optional<int64_t> staticLength(const SemanticType& type)
    {
        if (!isArray(type) || type.dims.empty() || type.dims.size() != type.info->indexTypes.size())
            return std::nullopt;

        int64_t total = 1;
        for (const Bounds& dim : type.dims)
            total *= dim.length();
        return total;
    }

    // ---- Diagnostics text -----------------------------------------------------------------------

    std::string describeBounds(const Bounds& bounds)
    {
        return std::to_string(bounds.left) + (bounds.ascending ? " to " : " downto ") + std::to_string(bounds.right);
    }

    std::string describe(const SemanticType& type)
    {
        if (!type.info)
            return "<no type>";

        std::string text = type.info->name;

        if (isArray(type) && !type.dims.empty())
        {
            text += "(";
            for (size_t i = 0; i < type.dims.size(); ++i)
                text += (i ? ", " : "") + describeBounds(type.dims[i]);
            text += ")";
        }
        else if (isArray(type) && type.unknownBounds)
        {
            text += "(...)";
        }
        else if (isScalar(type) && rangeDiffersFromDeclared(type))
        {
            text += " range " + describeScalarRange(type);
        }

        return text;
    }

} // namespace Pulse::Parser
