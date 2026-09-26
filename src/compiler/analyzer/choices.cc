#include "analyzer_internal.h"
#include "intervals.h"

#include <set>

namespace Pulse::Parser
{
    namespace
    {
        bool isOthers(const Expression& choice)
        {
            return dynamic_cast<const OthersExpr*>(&choice) != nullptr;
        }

        bool isOthersList(const ChoiceListExpr& list)
        {
            return list.alternatives.size() == 1 && isOthers(*list.alternatives.front());
        }

        /// The values a discrete selector can take: its subtype range, or the declared range of its type.
        ScalarRange selectorRange(const SemanticType& selector)
        {
            return selector.range.present ? selector.range : selector.info->range;
        }

        std::string valueName(const SemanticType& selector, int64_t value)
        {
            const TypeInfo& info = *selector.info;
            if (info.cls == TypeClass::Enumeration && value >= 0 && value < static_cast<int64_t>(info.literals.size()))
                return info.literals[value];
            return std::to_string(value);
        }

        /// Two matching patterns overlap when some value matches both: at every position the characters agree or one is '-'.
        bool patternsOverlap(const std::string& a, const std::string& b)
        {
            for (size_t i = 0; i < a.size(); ++i)
                if (a[i] != b[i] && a[i] != '-' && b[i] != '-')
                    return false;
            return true;
        }
    } // anonymous namespace

    const ChoiceListExpr& AnalyzerContext::requireChoiceList(const Expression& formal)
    {
        auto* list = dynamic_cast<const ChoiceListExpr*>(&formal);
        if (!list)
            fail("Expected a list of choices before '=>'", formal);

        for (const auto& choice : list->alternatives)
            if (isOthers(*choice) && !isOthersList(*list))
                fail("'others' must be the only choice of its list", *choice);

        return *list;
    }

    void AnalyzerContext::analyzeChoiceLists(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                             const ASTNode& at, const std::string& what, bool matching)
    {
        for (size_t i = 0; i + 1 < lists.size(); ++i)
            if (isOthersList(*lists[i]))
                fail("'others' must be the last choice of " + what, at);

        if (matching)
            analyzeMatchingChoices(lists, selector, at, what);
        else if (isOneDimensionalArray(selector))
            analyzeArrayChoices(lists, selector, at, what);
        else
            analyzeDiscreteChoices(lists, selector, at, what);
    }

    // ---- Integer and enumeration selectors ------------------------------------------------------

    void AnalyzerContext::analyzeDiscreteChoices(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                                 const ASTNode& at, const std::string& what)
    {
        if (!isDiscrete(selector) || isUniversal(selector))
            fail("The selector of " + what + " must be an integer, an enumeration or an array of characters, but it has type '"
                 + describe(selector) + "'", at);

        const ScalarRange range = selectorRange(selector);
        std::vector<Interval> intervals;
        bool hasOthers = false;

        for (const ChoiceListExpr* list : lists)
        {
            for (const auto& choice : list->alternatives)
            {
                if (isOthers(*choice))
                {
                    hasOthers = true;
                    continue;
                }

                if (isDiscreteRange(*choice))
                {
                    const Bounds bounds = requireStaticBounds(analyzeRange(*choice, &selector), *choice, "A choice range");
                    if (bounds.length() == 0 || bounds.low() < range.low || bounds.high() > range.high)
                        fail("The choice range '" + describeBounds(bounds) + "' is empty or outside the values of '" + describe(selector) + "'", *choice);
                    intervals.push_back({ bounds.low(), bounds.high() });
                    continue;
                }

                const SemanticType type = exprType(*choice, &selector);
                checkAssignable(selector, type, choice.get(), *choice, "a choice of " + what);

                auto value = fold(*choice, &selector);
                if (!value)
                    fail("The choices of " + what + " must be constant values", *choice);
                intervals.push_back({ value->integer, value->integer });
            }
        }

        if (auto twice = findOverlap(intervals))
            fail("The choice " + valueName(selector, *twice) + " appears more than once in " + what, at);

        if (hasOthers)
            return;

        if (auto gap = firstGap(intervals, range.low, range.high))
            fail("The choices of " + what + " do not cover every value of '" + describe(selector) + "' (" + valueName(selector, *gap)
                 + " is missing); add it or an 'others' choice", at);
    }

    // ---- Array selectors ------------------------------------------------------------------------

    void AnalyzerContext::analyzeArrayChoices(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                              const ASTNode& at, const std::string& what)
    {
        if (selector.info->element.info->cls != TypeClass::Enumeration)
            fail("The selector of " + what + " must be an integer, an enumeration or an array of characters, but it has type '"
                 + describe(selector) + "'", at);

        std::set<std::string> seen;
        bool hasOthers = false;

        for (const ChoiceListExpr* list : lists)
        {
            for (const auto& choice : list->alternatives)
            {
                if (isOthers(*choice))
                {
                    hasOthers = true;
                    continue;
                }

                auto* literal = dynamic_cast<const StringLiteralExpr*>(choice.get());
                if (!literal)
                    fail("The choices of " + what + " over '" + describe(selector) + "' must be string literals", *choice);

                exprType(*choice, &selector);
                if (!seen.insert(literal->value).second)
                    fail("The same string literal appears more than once in " + what, *choice);
            }
        }

        if (!hasOthers)
            fail("The choices of " + what + " over '" + describe(selector) + "' cannot list every value; end them with an 'others' choice", at);
    }

    // ---- Matching selectors (case? and select?) -------------------------------------------------

    /// A matching choice is a pattern over '0', '1' and '-' (which matches both); patterns must not overlap, and together
    /// they cover the selector when the values they match add up to all of them.
    void AnalyzerContext::analyzeMatchingChoices(const std::vector<const ChoiceListExpr*>& lists, const SemanticType& selector,
                                                 const ASTNode& at, const std::string& what)
    {
        const bool scalar = selector.info == m_std.stdLogic;
        const bool vector = isOneDimensionalArray(selector) && selector.info->element.info == m_std.stdLogic;
        if (!scalar && !vector)
            fail("The selector of " + what + " must be a std_logic or an array of std_logic to be matched with '?', but it has type '"
                 + describe(selector) + "'", at);

        std::vector<std::string> patterns;
        bool hasOthers = false;

        for (const ChoiceListExpr* list : lists)
        {
            for (const auto& choice : list->alternatives)
            {
                if (isOthers(*choice))
                {
                    hasOthers = true;
                    continue;
                }

                const SemanticType type = exprType(*choice, &selector);
                checkAssignable(selector, type, choice.get(), *choice, "a choice of " + what);

                std::string pattern;
                if (auto* character = dynamic_cast<const CharacterLiteralExpr*>(choice.get()); character && scalar)
                    pattern = character->value.substr(1, character->value.size() - 2);
                else if (auto* literal = dynamic_cast<const StringLiteralExpr*>(choice.get()); literal && vector)
                    pattern = literal->value;
                else
                    fail("The choices of " + what + " must be literals of the selector's type", *choice);

                if (pattern.find_first_not_of("01-") != std::string::npos)
                    fail("A matching choice can only contain '0', '1' and '-', but this one has '" + pattern + "'", *choice);

                patterns.push_back(pattern);
            }
        }

        for (size_t i = 0; i < patterns.size(); ++i)
            for (size_t j = i + 1; j < patterns.size(); ++j)
                if (patternsOverlap(patterns[i], patterns[j]))
                    fail("The choices \"" + patterns[i] + "\" and \"" + patterns[j] + "\" of " + what + " can match the same value", at);

        if (hasOthers)
            return;

        // Exact count: the patterns are disjoint, so they cover everything when the values they match add up to all of them.
        const std::optional<int64_t> width = scalar ? std::optional<int64_t>(1) : staticLength(selector);
        if (!width || *width > 62)
            fail("The choices of " + what + " cannot be shown to cover every value of '" + describe(selector) + "'; end them with an 'others' choice", at);

        uint64_t covered = 0;
        for (const std::string& pattern : patterns)
            covered += uint64_t(1) << std::count(pattern.begin(), pattern.end(), '-');

        if (covered != (uint64_t(1) << *width))
            fail("The choices of " + what + " do not cover every value of '" + describe(selector) + "'; add the missing ones or an 'others' choice", at);
    }

} // namespace Pulse::Parser
