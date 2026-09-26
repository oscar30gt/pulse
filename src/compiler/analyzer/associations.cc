#include "analyzer_internal.h"
#include "intervals.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace Pulse::Parser
{
    namespace
    {
        std::string noun(AssociationKind kind)
        {
            switch (kind)
            {
                case AssociationKind::PortMap:    return "port";
                case AssociationKind::GenericMap: return "generic";
                default:                          return "parameter";
            }
        }

        std::string capitalized(std::string text)
        {
            if (!text.empty())
                text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            return text;
        }

        std::optional<size_t> indexOf(const std::vector<FormalInfo>& formals, const std::string& name)
        {
            for (size_t i = 0; i < formals.size(); ++i)
                if (formals[i].name == name)
                    return i;
            return std::nullopt;
        }
    } // anonymous namespace

    std::vector<const Expression*> AnalyzerContext::associationsOf(const std::vector<ExpressionPtr>& list)
    {
        std::vector<const Expression*> pointers;
        pointers.reserve(list.size());
        for (const auto& item : list)
            pointers.push_back(item.get());
        return pointers;
    }

    /// The identifier a formal designator is built on: `p` in `p`, `p(3)`, `p(3 downto 0)` and `p.f(1)`.
    const SymbolExpr* AnalyzerContext::formalRoot(const Expression& formal) const
    {
        const Expression* current = &formal;
        while (current)
        {
            if (auto* symbol = dynamic_cast<const SymbolExpr*>(current)) return symbol;
            if (auto* call = dynamic_cast<const FunctionCallExpr*>(current)) current = call->callee.get();
            else if (auto* field = dynamic_cast<const FieldAccessExpr*>(current)) current = field->target.get();
            else return nullptr;
        }
        return nullptr;
    }

    // ---- Formal designators ---------------------------------------------------------------------

    /// A formal part names a formal whole (`p`), an element, slice or field of it (`p(3)`, `p.f`), or applies a conversion
    /// function to it (`to_integer(p)`).
    std::optional<AnalyzerContext::Designator> AnalyzerContext::designate(const std::vector<FormalInfo>& formals, const Expression& formalPart,
                                                                          std::string& problem) const
    {
        problem.clear();
        const SymbolExpr* root = formalRoot(formalPart);
        if (!root)
            return std::nullopt;

        Designator designator;

        if (dynamic_cast<const SymbolExpr*>(&formalPart))
        {
            auto index = indexOf(formals, root->name);
            if (!index)
            {
                problem = root->name;
                return std::nullopt;
            }
            designator.index = *index;
            return designator;
        }

        if (auto index = indexOf(formals, root->name))
        {
            designator.index = *index;
            designator.partial = true;
            return designator;
        }

        // The root is not a formal: `f(p)` applies the conversion `f` to the formal `p`.
        auto* call = dynamic_cast<const FunctionCallExpr*>(&formalPart);
        const SymbolExpr* inner = call && call->arguments.size() == 1 ? formalRoot(*call->arguments.front()) : nullptr;
        auto index = inner ? indexOf(formals, inner->name) : std::nullopt;
        if (!index)
        {
            problem = root->name;
            return std::nullopt;
        }

        designator.index = *index;
        designator.conversion = call;
        designator.partial = dynamic_cast<const SymbolExpr*>(call->arguments.front().get()) == nullptr;
        return designator;
    }

    // ---- Matching -------------------------------------------------------------------------------

    AssociationResult AnalyzerContext::matchAssociations(const std::vector<FormalInfo>& formals, const std::vector<const Expression*>& associations,
                                                         AssociationKind kind, const std::string& owner, const ASTNode& at)
    {
        AssociationResult result;
        const auto anchor = [&]() -> const ASTNode& { return associations.empty() ? at : *associations.front(); };
        result.associated.assign(formals.size(), false);
        std::vector<bool> whole(formals.size(), false);
        std::vector<bool> partial(formals.size(), false);
        bool sawNamed = false;
        size_t position = 0;

        const auto reject = [&](const std::string& problem, const ASTNode& at)
        {
            result.problem = problem;
            result.at = &at;
            result.bindings.clear();
            return result;
        };

        for (const Expression* association : associations)
        {
            Binding binding;
            binding.association = association;
            const auto* named = dynamic_cast<const NamedAssociationExpr*>(association);

            if (!named)
            {
                if (sawNamed)
                    return reject("A positional association cannot follow a named association in " + owner, *association);
                if (position >= formals.size())
                    return reject("Too many associations for " + owner + ": it has only " + std::to_string(formals.size()) + " " + noun(kind)
                                  + (formals.size() == 1 ? "" : "s"), *association);

                binding.formal = position++;
                binding.actual = association;
                binding.formalPart = association;
            }
            else
            {
                sawNamed = true;
                std::string problem;
                auto designator = designate(formals, *named->formal, problem);
                if (!designator)
                {
                    std::string available;
                    for (const FormalInfo& formal : formals)
                        available += (available.empty() ? "" : ", ") + formal.name;

                    return reject(problem.empty() ? "This is not a valid formal in " + owner
                                                  : capitalized(owner) + " has no " + noun(kind) + " named '" + problem + "' ("
                                                    + noun(kind) + "s: " + available + ")", *named->formal);
                }

                binding.formal = designator->index;
                binding.actual = named->actual.get();
                binding.formalPart = named->formal.get();
                binding.conversion = designator->conversion;
                binding.partial = designator->partial;
            }

            const FormalInfo& formal = formals[binding.formal];
            const bool isOpen = dynamic_cast<const OpenExpr*>(binding.actual) != nullptr;

            if (isOpen && binding.partial)
                return reject("'open' cannot stand for part of " + noun(kind) + " '" + formal.name + "'", *binding.actual);

            if (binding.partial)
            {
                if (whole[binding.formal])
                    return reject(capitalized(noun(kind)) + " '" + formal.name + "' of " + owner + " is associated both as a whole and element by element", *association);
                partial[binding.formal] = true;
            }
            else
            {
                if (whole[binding.formal] || partial[binding.formal])
                    return reject(capitalized(noun(kind)) + " '" + formal.name + "' of " + owner + " is "
                                  + (whole[binding.formal] ? "connected twice" : "associated both as a whole and element by element"), *association);
                whole[binding.formal] = true;
            }

            if (isOpen)
            {
                const bool outputPort = kind == AssociationKind::PortMap && formal.mode != PortMode::In;
                if (!formal.hasDefault && !outputPort)
                    return reject(capitalized(noun(kind)) + " '" + formal.name + "' of " + owner + " cannot be left open"
                                  + (kind == AssociationKind::PortMap ? "; it is an input without a default value" : "; it has no default value"), *binding.actual);
            }

            result.associated[binding.formal] = true;
            result.bindings.push_back(binding);
        }

        // Every formal that needs a value must have one.
        for (size_t i = 0; i < formals.size(); ++i)
        {
            if (result.associated[i] || formals[i].hasDefault)
                continue;

            const bool optional = kind == AssociationKind::PortMap && formals[i].mode != PortMode::In;
            if (optional)
                continue;

            if (kind == AssociationKind::PortMap)
                return reject("Input port '" + formals[i].name + "' of " + owner + " is not connected", anchor());
            return reject(capitalized(noun(kind)) + " '" + formals[i].name + "' of " + owner + " is not associated and has no default value",
                          anchor());
        }

        // Element-by-element associations of a formal must not overlap, and an input must be covered completely.
        for (size_t i = 0; i < formals.size(); ++i)
        {
            if (!partial[i])
                continue;

            std::vector<const Binding*> parts;
            for (const Binding& binding : result.bindings)
                if (binding.formal == i)
                    parts.push_back(&binding);

            const std::string problem = partialCoverageProblem(formals[i], parts);
            if (!problem.empty())
                return reject(problem + " (" + owner + ")", *parts.front()->association);
        }

        return result;
    }

    // ---- Element-by-element associations --------------------------------------------------------

    /// Decides, when the elements are constants, whether the associated elements of one formal overlap or leave a gap.
    std::string AnalyzerContext::partialCoverageProblem(const FormalInfo& formal, const std::vector<const Binding*>& parts)
    {
        const bool needsAll = formal.mode == PortMode::In && !formal.hasDefault;

        if (isRecord(formal.type))
        {
            std::set<std::string> covered;
            for (const Binding* part : parts)
            {
                auto* field = dynamic_cast<const FieldAccessExpr*>(part->conversion ? part->conversion->arguments.front().get() : part->formalPart);
                if (!field)
                    return "";      // an element of a field: cannot be decided here
                if (!covered.insert(field->fieldName).second)
                    return "Field '" + field->fieldName + "' of '" + formal.name + "' is associated more than once";
            }

            if (needsAll)
                for (const auto& field : formal.type.info->fields)
                    if (!covered.count(field.name))
                        return "Field '" + field.name + "' of '" + formal.name + "' is not associated";
            return "";
        }

        if (!isOneDimensionalArray(formal.type) || formal.type.dims.empty())
            return "";

        const Bounds& dim = formal.type.dims.front();
        std::vector<Interval> intervals;
        for (const Binding* part : parts)
        {
            const Expression* designator = part->conversion ? part->conversion->arguments.front().get() : part->formalPart;
            auto* call = dynamic_cast<const FunctionCallExpr*>(designator);
            if (!call || call->arguments.size() != 1)
                return "";

            const Expression& argument = *call->arguments.front();
            if (isDiscreteRange(argument))
            {
                const RangeInfo range = analyzeRange(argument, &formal.type.info->indexTypes.front());
                if (!range.bounds)
                    return "";
                intervals.push_back({ range.bounds->low(), range.bounds->high() });
            }
            else
            {
                auto index = fold(argument, &formal.type.info->indexTypes.front());
                if (!index)
                    return "";
                intervals.push_back({ index->integer, index->integer });
            }
        }

        for (const Interval& interval : intervals)
            if (interval.low < dim.low() || interval.high > dim.high())
                return "Element " + std::to_string(interval.low < dim.low() ? interval.low : interval.high) + " is outside '" + formal.name + "'";

        if (auto twice = findOverlap(intervals))
            return "Element " + std::to_string(*twice) + " of '" + formal.name + "' is associated more than once";

        if (needsAll && totalLength(intervals) != dim.length())
            return "Only " + std::to_string(totalLength(intervals)) + " of the " + std::to_string(dim.length()) + " elements of '" + formal.name + "' are associated";

        return "";
    }

} // namespace Pulse::Parser

namespace Pulse::Parser
{
    // ---- The type of a formal part --------------------------------------------------------------

    SemanticType AnalyzerContext::formalPartType(const FormalInfo& formal, const Expression& part)
    {
        if (dynamic_cast<const SymbolExpr*>(&part))
            return formal.type;

        if (auto* field = dynamic_cast<const FieldAccessExpr*>(&part))
        {
            const SemanticType record = formalPartType(formal, *field->target);
            if (!isRecord(record))
                fail("'" + field->fieldName + "' cannot be selected: the formal has type '" + describe(record) + "', which is not a record", part);

            for (const auto& candidate : record.info->fields)
                if (candidate.name == field->fieldName)
                    return candidate.type;
            fail("Record type '" + record.info->name + "' has no field '" + field->fieldName + "'", part);
        }

        auto* call = dynamic_cast<const FunctionCallExpr*>(&part);
        if (!call)
            fail("This is not a valid formal designator", part);

        const SemanticType array = formalPartType(formal, *call->callee);
        if (!isArray(array))
            fail("A value of type '" + describe(array) + "' cannot be indexed or sliced; only arrays can", part);

        // A slice keeps the array type, with the bounds of the slice when they are known.
        if (call->arguments.size() == 1 && isRangeArgument(*call->arguments.front()))
        {
            SemanticType slice;
            slice.info = array.info;
            const RangeInfo range = analyzeRange(*call->arguments.front(), &array.info->indexTypes.front());
            if (range.bounds)
                slice.dims = { *range.bounds };
            else
                slice.unknownBounds = range.dependsOnGenerics;
            return slice;
        }

        if (call->arguments.size() != array.info->indexTypes.size())
            fail("'" + describe(array) + "' needs " + std::to_string(array.info->indexTypes.size()) + " index value(s), but "
                 + std::to_string(call->arguments.size()) + " were given", part);

        return array.info->element;
    }

    SemanticType AnalyzerContext::conversionResultType(const FunctionCallExpr& conversion, const SemanticType& partType,
                                                       const SemanticType* expected)
    {
        auto* name = dynamic_cast<const SymbolExpr*>(conversion.callee.get());
        const Symbol* symbol = name ? find(name->name) : nullptr;
        if (!symbol)
            fail("The conversion of a formal must name a type or a function", conversion);

        if (symbol->kind == SymbolKind::Type)
        {
            if (!closelyRelated(partType, symbol->type))
                fail("Cannot convert '" + describe(partType) + "' to '" + describe(symbol->type) + "': the types are not closely related", conversion);

            SemanticType result = symbol->type;
            if (isArray(result) && !isConstrainedArray(result))
            {
                result.dims = partType.dims;                // `std_logic_vector(u)` has the length of u
                result.unknownBounds = partType.unknownBounds;
            }
            return result;
        }

        if (symbol->kind != SymbolKind::Subprogram)
            fail("'" + name->name + "' is neither a type nor a function, so it cannot convert a formal", conversion);

        std::vector<const SubprogramInfo*> fitting;
        for (const SubprogramInfo* candidate : visibleSubprograms(name->name))
        {
            const bool fits = candidate->isFunction && candidate->parameters.size() == 1
                           && assignmentProblem(candidate->parameters.front().type, partType, "").empty();
            if (fits && (!expected || !expected->valid() || candidate->returnType.info == expected->info))
                fitting.push_back(candidate);
        }

        if (fitting.empty())
            fail("No function '" + name->name + "' converts a value of type '" + describe(partType) + "'", conversion);
        if (fitting.size() > 1)
            fail("The conversion '" + name->name + "' is ambiguous for a value of type '" + describe(partType) + "': " + describeOverloads(fitting), conversion);

        m_resolvedCalls[&conversion] = fitting.front();
        return fitting.front()->returnType;
    }

    // ---- Checking one association ---------------------------------------------------------------

    void AnalyzerContext::checkBinding(const Binding& binding, const FormalInfo& formal, AssociationKind kind, const std::string& what)
    {
        const Expression& actual = *binding.actual;
        if (dynamic_cast<const OpenExpr*>(&actual))
            return;                     // the association engine already decided whether `open` is allowed

        const bool output = formal.mode != PortMode::In;
        const Expression* designator = binding.conversion ? binding.conversion->arguments.front().get() : binding.formalPart;
        SemanticType formalType = binding.partial || binding.conversion ? formalPartType(formal, *designator) : formal.type;

        if (binding.conversion)
        {
            if (!output)
                fail("The formal of " + what + " is an input, so a conversion belongs on the actual, not on the formal", *binding.formalPart);
            formalType = conversionResultType(*binding.conversion, formalType, nullptr);
        }

        // ---- What the kind of formal asks of the actual ----
        if (kind == AssociationKind::GenericMap)
        {
            if (!isStaticExpression(actual))
                fail("The actual of " + what + " must be constant: it cannot read signals, ports or variables", actual);
        }
        else if (formal.kind == SymbolKind::Signal && kind == AssociationKind::Call)
        {
            const RootObject root = rootObject(actual);
            if (!root || root.kind != SymbolKind::Signal)
                fail("The actual of " + what + " must be a signal, because the parameter is declared 'signal'", actual);
        }
        else if (formal.kind == SymbolKind::Variable)
        {
            const RootObject root = rootObject(actual);
            if (!root || root.kind != SymbolKind::Variable)
                fail("The actual of " + what + " must be a variable, because the parameter is declared 'variable'", actual);
        }

        // ---- Types ----
        const SemanticType actualType = exprType(actual, &formalType);

        if (!output)
        {
            checkAssignable(formalType, actualType, &actual, actual, what);
            return;
        }

        if (kind == AssociationKind::Call && formal.kind == SymbolKind::Variable)
        {
            const RootObject root = rootObject(actual);
            if (root.mode == PortMode::In)
                fail("The actual of " + what + " is an input and cannot receive the output", actual);
            checkObjectAccess(root, actual, true);
        }
        else
        {
            checkWritable(actual, "the actual of " + what);
        }

        checkAssignable(actualType, formalType, nullptr, actual, what);
        if (formal.mode == PortMode::InOut)
            checkAssignable(formalType, actualType, nullptr, actual, what);
    }

} // namespace Pulse::Parser
