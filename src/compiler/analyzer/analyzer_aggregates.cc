#include "analyzer_internal.h"
#include "intervals.h"

#include <algorithm>

namespace Pulse::Parser
{
    namespace
    {
        /// Which indices an array aggregate defines: intervals from named choices and the leading positional run.
        struct Coverage
        {
            std::vector<Interval> intervals;
            int64_t positional = 0;
            bool hasOthers = false;
            bool hasNamed = false;
        };

        Interval positionalInterval(const Bounds& dim, int64_t count)
        {
            return dim.ascending ? Interval{ dim.low(), dim.low() + count - 1 } : Interval{ dim.high() - count + 1, dim.high() };
        }

        /// The index range an aggregate ends up with, or the sentence explaining why it is invalid.
        struct AggregateShape
        {
            std::vector<Bounds> dims;
            std::string problem;
        };

        AggregateShape shapeOfConstrained(const SemanticType& expected, Coverage& coverage)
        {
            const Bounds& dim = expected.dims.front();
            const std::string typeName = "'" + describe(expected) + "'";

            if (coverage.positional > dim.length())
                return { {}, "The aggregate has " + std::to_string(coverage.positional) + " positional elements, but " + typeName
                             + " has only " + std::to_string(dim.length()) };

            std::vector<Interval> intervals = coverage.intervals;
            if (coverage.positional > 0)
                intervals.push_back(positionalInterval(dim, coverage.positional));

            for (const Interval& i : intervals)
                if (i.low < dim.low() || i.high > dim.high())
                    return { {}, "Index " + std::to_string(i.low < dim.low() ? i.low : i.high) + " of the aggregate is outside the range of " + typeName };

            if (auto twice = findOverlap(intervals))
                return { {}, "Index " + std::to_string(*twice) + " is defined twice in the aggregate" };

            if (!coverage.hasOthers && totalLength(intervals) != dim.length())
                return { {}, "The aggregate defines " + std::to_string(totalLength(intervals)) + " of the " + std::to_string(dim.length())
                             + " elements of " + typeName + "; define the missing ones or add an 'others' choice" };

            return { expected.dims, {} };
        }

        /// With no constraint to take from, an aggregate starts at the left bound of its index type and runs in the direction of
        /// that type, which for the index types this analyzer models is upwards.
        AggregateShape shapeOfUnconstrained(Coverage& coverage, const SemanticType& indexType)
        {
            if (coverage.hasOthers)
                return { {}, "'others' can only be used when the target type has a known length" };

            if (!coverage.hasNamed)
            {
                const int64_t first = indexType.range.present ? indexType.range.low : 0;
                return { { Bounds{ first, first + coverage.positional - 1, true } }, {} };
            }

            if (coverage.positional > 0)
                return { {}, "Positional and named elements cannot be mixed when the target type has no index range" };

            std::vector<Interval> intervals = coverage.intervals;
            if (auto twice = findOverlap(intervals))
                return { {}, "Index " + std::to_string(*twice) + " is defined twice in the aggregate" };

            const int64_t low = intervals.front().low;
            int64_t high = intervals.front().high;
            for (const Interval& i : intervals) high = std::max(high, i.high);

            if (totalLength(intervals) != high - low + 1)
                return { {}, "The named elements of the aggregate leave gaps between " + std::to_string(low) + " and " + std::to_string(high) };

            return { { Bounds{ low, high, true } }, {} };
        }

        /// One element of an aggregate: a positional value, or `choices => value`.
        struct ElementView
        {
            const Expression* value = nullptr;      ///< The value assigned
            const Expression* formal = nullptr;     ///< What stands before `=>`, or null for a positional element
            bool positional() const { return formal == nullptr; }
        };

        ElementView viewOf(const Expression& element)
        {
            ElementView view;
            if (auto* named = dynamic_cast<const NamedAssociationExpr*>(&element))
            {
                view.value = named->actual.get();
                view.formal = named->formal.get();
            }
            else
            {
                view.value = &element;
            }
            return view;
        }

        bool isOthersList(const ChoiceListExpr& list)
        {
            return list.alternatives.size() == 1 && dynamic_cast<const OthersExpr*>(list.alternatives.front().get());
        }
    } // anonymous namespace

    SemanticType AnalyzerContext::typeOfAggregate(const AggregateExpr& expr, const SemanticType* expected)
    {
        if (!expected)
            fail("The type of this aggregate cannot be determined from its context", expr);

        if (isArray(*expected)) return arrayAggregate(expr, *expected);
        if (isRecord(*expected)) return recordAggregate(expr, *expected);

        fail("An aggregate cannot be used where '" + describe(*expected) + "' is expected", expr);
    }

    // ---- Arrays ---------------------------------------------------------------------------------

    SemanticType AnalyzerContext::arrayAggregate(const AggregateExpr& expr, const SemanticType& expected)
    {
        if (!isOneDimensionalArray(expected))
            fail("Aggregates of multi-dimensional arrays are not supported yet", expr);

        const SemanticType& elementType = expected.info->element;
        const SemanticType& indexType = expected.info->indexTypes.front();
        const std::string context = "an element of the aggregate for '" + expected.info->name + "'";

        Coverage coverage;
        for (const auto& element : expr.elements)
        {
            const ElementView view = viewOf(*element);

            const SemanticType valueType = exprType(*view.value, &elementType);
            checkAssignable(elementType, valueType, view.value, *view.value, context);

            if (view.positional())
            {
                if (coverage.hasNamed || coverage.hasOthers)
                    fail("Positional elements must come before named elements in an aggregate", *view.value);
                ++coverage.positional;
                continue;
            }

            const ChoiceListExpr& choices = requireChoiceList(*view.formal);
            coverage.hasNamed = true;
            coverage.hasOthers = coverage.hasOthers || isOthersList(choices);

            for (const auto& choice : choices.alternatives)
            {
                if (dynamic_cast<const OthersExpr*>(choice.get()))
                    continue;

                if (isDiscreteRange(*choice))
                {
                    Bounds b = requireStaticBounds(analyzeRange(*choice, &indexType), *choice, "An aggregate choice range");
                    coverage.intervals.push_back({ b.low(), b.high() });
                }
                else
                {
                    exprType(*choice, &indexType);
                    const int64_t index = requireStaticInteger(*choice, "An aggregate choice", &indexType);
                    coverage.intervals.push_back({ index, index });
                }
            }
        }

        // The bounds of the target depend on a generic: elements and choices were checked, coverage cannot be.
        if (expected.unknownBounds)
        {
            std::vector<Interval> intervals = coverage.intervals;
            if (auto twice = findOverlap(intervals))
                fail("Index " + std::to_string(*twice) + " is defined twice in the aggregate", expr);
            return expected;
        }

        AggregateShape shape = expected.dims.empty() ? shapeOfUnconstrained(coverage, indexType) : shapeOfConstrained(expected, coverage);
        if (!shape.problem.empty())
            fail(shape.problem, expr);

        SemanticType result;
        result.info = expected.info;
        result.dims = std::move(shape.dims);
        return result;
    }

    // ---- Records --------------------------------------------------------------------------------

    SemanticType AnalyzerContext::recordAggregate(const AggregateExpr& expr, const SemanticType& expected)
    {
        const auto& fields = expected.info->fields;
        std::vector<bool> assigned(fields.size(), false);
        size_t nextPositional = 0;

        const auto assign = [&](size_t index, const Expression& value)
        {
            if (assigned[index])
                fail("Field '" + fields[index].name + "' is assigned twice in the aggregate", value);

            assigned[index] = true;
            const SemanticType valueType = exprType(value, &fields[index].type);
            checkAssignable(fields[index].type, valueType, &value, value,
                            "field '" + fields[index].name + "' of record '" + expected.info->name + "'");
        };

        for (const auto& item : expr.elements)
        {
            const ElementView element = viewOf(*item);

            if (element.positional())
            {
                if (nextPositional >= fields.size())
                    fail("The aggregate has more elements than record '" + expected.info->name + "' has fields", *element.value);
                assign(nextPositional++, *element.value);
                continue;
            }

            const ChoiceListExpr& choices = requireChoiceList(*element.formal);
            for (const auto& choice : choices.alternatives)
            {
                if (dynamic_cast<const OthersExpr*>(choice.get()))
                    continue;

                auto* name = dynamic_cast<const SymbolExpr*>(choice.get());
                auto field = name ? std::find_if(fields.begin(), fields.end(), [&](const auto& f) { return f.name == name->name; }) : fields.end();
                if (field == fields.end())
                    fail("Record type '" + expected.info->name + "' has no field named in this choice", *choice);
                assign(static_cast<size_t>(field - fields.begin()), *element.value);
            }

            if (isOthersList(choices))
                for (size_t i = 0; i < fields.size(); ++i)
                    if (!assigned[i]) assign(i, *element.value);
        }

        std::string missing;
        for (size_t i = 0; i < fields.size(); ++i)
            if (!assigned[i]) missing += (missing.empty() ? "" : ", ") + fields[i].name;

        if (!missing.empty())
            fail("The aggregate does not assign field(s) " + missing + " of record '" + expected.info->name + "'", expr);

        return expected;
    }

} // namespace Pulse::Parser
