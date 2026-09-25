#include "analyzer_internal.h"
#include "checked_math.h"

#include <algorithm>
#include <unordered_set>

namespace Pulse::Parser
{
    // ---- Scalars --------------------------------------------------------------------------------

    void AnalyzerContext::defineNumeric(const NumericTypeDefinition& def, TypeInfo& info)
    {
        info.range = analyzeScalarRange(*def.range, info.cls, def);
    }

    void AnalyzerContext::defineEnumeration(const EnumeratedTypeDefinition& def, TypeInfo& info)
    {
        info.cls = TypeClass::Enumeration;

        std::unordered_set<std::string> seen;
        for (const std::string& literal : def.literals)
        {
            if (!seen.insert(literal).second)
                fail("Enumeration literal " + literal + " is declared twice in type '" + info.name + "'", def);

            const bool isCharacterLiteral = literal.front() == '\'';
            if (!isCharacterLiteral && m_scopes.back().symbols.count(literal))
                fail("Enumeration literal '" + literal + "' conflicts with an existing declaration in this region", def);
        }

        info.literals = def.literals;
        info.range = { true, 0, static_cast<int64_t>(def.literals.size()) - 1, 0.0, 0.0 };
    }

    void AnalyzerContext::definePhysical(const PhysicalTypeDefinition& def, TypeInfo& info)
    {
        TypeClass cls = TypeClass::Integer;
        info.range = analyzeScalarRange(*def.range, cls, def);
        if (cls != TypeClass::Integer)
            fail("The range of a physical type must use integer bounds", def);

        info.cls = TypeClass::Physical;

        for (const auto& unit : def.units)
        {
            const auto sameName = [&](const TypeInfo::Unit& u) { return u.name == unit->name; };
            if (std::any_of(info.units.begin(), info.units.end(), sameName))
                fail("Unit '" + unit->name + "' is declared twice in type '" + info.name + "'", def);

            if (m_scopes.back().symbols.count(unit->name))
                fail("Unit '" + unit->name + "' conflicts with an existing declaration in this region", def);

            // The base unit has no definition; every other unit is `name = [multiplier] earlier_unit`.
            int64_t factor = 1;
            if (!unit->ofUnit.empty())
            {
                const int64_t multiplier = unit->multiplier ? requireStaticInteger(*unit->multiplier, "A unit multiplier") : 1;
                auto base = std::find_if(info.units.begin(), info.units.end(),
                                         [&](const TypeInfo::Unit& u) { return u.name == unit->ofUnit; });
                if (base == info.units.end())
                    fail("Unit '" + unit->name + "' is defined in terms of '" + unit->ofUnit + "', which is not an earlier unit of this type", def);

                auto scaled = checkedMul(multiplier, base->factor);
                if (!scaled || *scaled <= 0)
                    fail("Unit '" + unit->name + "' is too large or not positive", def);
                factor = *scaled;
            }
            info.units.push_back({ unit->name, factor });
        }
    }

    // ---- Composites -----------------------------------------------------------------------------

    void AnalyzerContext::defineConstrainedArray(const ArrayTypeDefinition& def, TypeInfo& info)
    {
        info.cls = TypeClass::Array;

        for (const auto& range : def.indexRanges)
        {
            RangeInfo analyzed = analyzeRange(*range);
            info.indexTypes.push_back(analyzed.type);

            if (!analyzed.bounds && analyzed.dependsOnGenerics)
            {
                info.declaredUnknownBounds = true;      // the bounds depend on a generic
                continue;
            }
            info.declaredDims.push_back(requireStaticBounds(analyzed, *range, "An array index range"));
        }

        if (info.declaredUnknownBounds)
            info.declaredDims.clear();

        info.element = requireElementType(*def.elementType, info.name);
        info.isResolved = info.element.info->isResolved || info.element.resolved;
    }

    void AnalyzerContext::defineUnconstrainedArray(const UnconstrainedArrayTypeDefinition& def, TypeInfo& info)
    {
        info.cls = TypeClass::Array;

        for (const std::string& mark : def.indexTypeMarks)
        {
            SemanticType indexType = resolveTypeName(mark, def);
            if (!isDiscrete(indexType))
                fail("The index type of array '" + info.name + "' must be an integer or enumeration type, but '" + mark + "' is not", def);
            info.indexTypes.push_back(indexType);
        }

        info.element = requireElementType(*def.elementType, info.name);
        info.isResolved = info.element.info->isResolved || info.element.resolved;
    }

    void AnalyzerContext::defineRecord(const RecordTypeDefinition& def, TypeInfo& info)
    {
        info.cls = TypeClass::Record;

        info.isResolved = true;     // a record is resolved when every field is
        std::unordered_set<std::string> seen;
        for (const auto& field : def.fields)
        {
            if (!seen.insert(field->name).second)
                fail("Field '" + field->name + "' is declared twice in record '" + info.name + "'", def);

            info.fields.push_back({ field->name, requireElementType(*field->type, info.name) });
            info.isResolved = info.isResolved && (info.fields.back().type.info->isResolved || info.fields.back().type.resolved);
        }
    }

} // namespace Pulse::Parser
