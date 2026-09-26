#include "analyzer_internal.h"

#include <algorithm>
#include <unordered_set>

namespace Pulse::Parser
{
    namespace
    {
        /// How many parameters an overloaded operator may have; 0 when the name is not an overloadable operator.
        struct Arity { size_t low = 0; size_t high = 0; };

        Arity arityOf(const std::string& op)
        {
            static const std::unordered_set<std::string> binaryOnly = {
                "=", "/=", "<", "<=", ">", ">=", "?=", "?/=", "?<", "?<=", "?>", "?>=",
                "sll", "srl", "sla", "sra", "rol", "ror", "&", "*", "/", "mod", "rem", "**",
            };
            static const std::unordered_set<std::string> unaryOrBinary = { "and", "or", "nand", "nor", "xor", "xnor", "+", "-" };
            static const std::unordered_set<std::string> unaryOnly = { "abs", "not", "??" };

            if (binaryOnly.count(op))    return { 2, 2 };
            if (unaryOrBinary.count(op)) return { 1, 2 };
            if (unaryOnly.count(op))     return { 1, 1 };
            return {};
        }
    } // anonymous namespace

    // ---- Profiles -------------------------------------------------------------------------------

    bool AnalyzerContext::sameProfile(const SubprogramInfo& a, const SubprogramInfo& b)
    {
        if (a.isFunction != b.isFunction || a.parameters.size() != b.parameters.size())
            return false;

        for (size_t i = 0; i < a.parameters.size(); ++i)
            if (a.parameters[i].type.info != b.parameters[i].type.info)
                return false;

        return !a.isFunction || a.returnType.info == b.returnType.info;
    }

    /// The parameter classes and modes of a specification, with the defaults the LRM gives them: a parameter of mode `in` is a
    /// constant, one of another mode is a variable.
    SubprogramInfo AnalyzerContext::resolveSubprogramSpec(const SubprogramSpec& spec)
    {
        SubprogramInfo info;
        info.name = spec.name;
        info.isFunction = spec.kind == SubprogramKind::Function;
        info.impure = spec.impure;
        info.location = spec.source;

        const std::string owner = std::string(info.isFunction ? "function" : "procedure") + " '" + spec.name + "'";
        std::unordered_set<std::string> names;

        for (const auto& parameter : spec.parameters)
        {
            if (!names.insert(parameter->name).second)
                fail("Parameter '" + parameter->name + "' is declared twice in " + owner, *parameter);

            FormalInfo formal;
            formal.name = parameter->name;
            formal.mode = parameter->mode;
            formal.location = parameter->source;
            formal.hasDefault = parameter->defaultValue != nullptr;

            switch (parameter->objectClass)
            {
                case ParameterClass::Constant: formal.kind = SymbolKind::Constant; break;
                case ParameterClass::Signal:   formal.kind = SymbolKind::Signal; break;
                case ParameterClass::Variable: formal.kind = SymbolKind::Variable; break;
                default: formal.kind = formal.mode == PortMode::In ? SymbolKind::Constant : SymbolKind::Variable; break;
            }

            if (formal.kind == SymbolKind::Constant && formal.mode != PortMode::In)
                fail("The constant parameter '" + parameter->name + "' of " + owner + " must have mode 'in'", *parameter);

            if (info.isFunction && formal.mode != PortMode::In)
                fail("The parameters of " + owner + " must have mode 'in'", *parameter);

            if (info.isFunction && formal.kind == SymbolKind::Variable)
                fail("A parameter of " + owner + " cannot be a variable", *parameter);

            formal.type = resolveTypeSpec(*parameter->typeSpec);

            if (parameter->defaultValue)
            {
                if (formal.mode != PortMode::In)
                    fail("Only a parameter of mode 'in' can have a default value, but '" + parameter->name + "' is not", *parameter);
                checkInitialValue(*parameter->defaultValue, formal.type, "parameter '" + parameter->name + "' of " + owner);
            }

            info.parameters.push_back(std::move(formal));
        }

        if (spec.returnType)
            info.returnType = resolveTypeSpec(*spec.returnType);

        return info;
    }

    void AnalyzerContext::checkOperatorProfile(const SubprogramInfo& profile, const ASTNode& at) const
    {
        if (profile.name.empty() || profile.name.front() != '"')
            return;

        const std::string op = profile.name.substr(1, profile.name.size() - 2);
        const Arity arity = arityOf(op);

        if (arity.high == 0)
            fail("\"" + op + "\" is not an operator that can be overloaded", at);

        if (!profile.isFunction)
            fail("Only a function can overload the operator \"" + op + "\"", at);

        if (profile.parameters.size() < arity.low || profile.parameters.size() > arity.high)
            fail("The operator \"" + op + "\" takes " + (arity.low == arity.high ? std::to_string(arity.low) : "1 or 2")
                 + " parameter(s), but this function has " + std::to_string(profile.parameters.size()), at);
    }

    // ---- Registering ----------------------------------------------------------------------------

    /// Adds a subprogram to the overloads under its name in the current region. A body completes an earlier declaration with the
    /// same profile; any other repeated profile is an error.
    SubprogramInfo& AnalyzerContext::registerSubprogram(SubprogramInfo profile, const ASTNode& at, bool isBody)
    {
        Symbol* existing = findInCurrentScope(profile.name);
        if (existing && existing->kind != SymbolKind::Subprogram)
            fail("'" + profile.name + "' is already declared in this region", at);

        if (existing)
        {
            for (const SubprogramInfo* overload : existing->overloads)
            {
                if (!sameProfile(*overload, profile))
                    continue;

                if (!isBody)
                    fail("The subprogram '" + describeProfile(profile) + "' is already declared in this region", at);
                if (overload->hasBody)
                    fail("The subprogram '" + describeProfile(profile) + "' already has a body", at);
                if (overload->impure != profile.impure)
                    fail("The body of '" + describeProfile(profile) + "' is " + (profile.impure ? "impure" : "pure") + ", but it was declared "
                         + (overload->impure ? "impure" : "pure"), at);

                // The arena owns every SubprogramInfo as a mutable object; symbols only see them as const.
                return const_cast<SubprogramInfo&>(*overload);
            }
        }

        m_subprograms.push_back(std::make_unique<SubprogramInfo>(std::move(profile)));
        SubprogramInfo& info = *m_subprograms.back();

        if (existing)
        {
            existing->overloads.push_back(&info);
            return info;
        }

        Symbol symbol;
        symbol.kind = SymbolKind::Subprogram;
        symbol.overloads = { &info };
        declare(info.name, std::move(symbol), at);
        return info;
    }

    void AnalyzerContext::declareSubprogram(const SubprogramDeclaration& decl)
    {
        SubprogramInfo profile = resolveSubprogramSpec(*decl.spec);
        checkOperatorProfile(profile, decl);

        SubprogramInfo& info = registerSubprogram(std::move(profile), decl, false);
        m_pendingBodies.push_back({ &info, m_scopes.size() - 1, decl.source });
    }

    void AnalyzerContext::checkBodiesDefined()
    {
        const size_t depth = m_scopes.size() - 1;

        for (const PendingBody& pending : m_pendingBodies)
            if (pending.depth == depth && !pending.info->hasBody)
                fail("The subprogram '" + describeProfile(*pending.info) + "' is declared but has no body in this region", pending.location);

        m_pendingBodies.erase(std::remove_if(m_pendingBodies.begin(), m_pendingBodies.end(),
                                             [&](const PendingBody& pending) { return pending.depth == depth; }),
                              m_pendingBodies.end());
    }

    // ---- Bodies ---------------------------------------------------------------------------------

    void AnalyzerContext::declareSubprogramBody(const SubprogramBody& decl)
    {
        SubprogramInfo profile = resolveSubprogramSpec(*decl.spec);
        checkOperatorProfile(profile, decl);

        SubprogramInfo& info = registerSubprogram(profile, decl, true);
        info.hasBody = true;        // before the body is analyzed, so the subprogram can call itself
        analyzeSubprogramBody(decl, info, profile);
    }

    void AnalyzerContext::analyzeSubprogramBody(const SubprogramBody& decl, SubprogramInfo& info, const SubprogramInfo& profile)
    {
        SubprogramInfo* const outerSubprogram = m_subprogram;
        const bool outerInProcess = m_bodyInProcess;
        const size_t outerDepth = m_bodyDepth;
        std::vector<std::string> outerLoops = std::move(m_loops);

        // A subprogram declared in a process may assign the signals that process drives; so may the ones nested in it.
        m_bodyInProcess = m_process != nullptr || (outerSubprogram && outerInProcess);
        m_subprogram = &info;
        m_loops.clear();

        pushScope();
        m_bodyDepth = m_scopes.size() - 1;

        for (const FormalInfo& parameter : profile.parameters)
        {
            Symbol symbol;
            symbol.kind = parameter.kind;
            symbol.type = parameter.type;
            symbol.mode = parameter.mode;
            symbol.isParameter = true;
            symbol.objectId = newObjectId();
            declare(parameter.name, std::move(symbol), parameter.location);
        }

        analyzeDeclarations(decl.declarations);
        analyzeSequence(decl.body);
        resolvePendingLabelSpecs();
        popScope();

        if (info.isFunction && !alwaysReturns(decl.body))
            fail("The function '" + info.name + "' can reach its end without a return statement", decl);

        m_subprogram = outerSubprogram;
        m_bodyInProcess = outerInProcess;
        m_bodyDepth = outerDepth;
        m_loops = std::move(outerLoops);
    }

    // ---- Signatures -----------------------------------------------------------------------------

    const SubprogramInfo& AnalyzerContext::selectBySignature(const std::vector<const SubprogramInfo*>& overloads,
                                                             const SignatureExpr& signature, const ASTNode& at)
    {
        const auto typeOfMark = [&](const Expression& mark)
        {
            auto* spec = dynamic_cast<const TypeSpec*>(&mark);
            if (!spec)
                fail("A signature lists type marks", mark);
            return resolveTypeName(spec->typeName, mark);
        };

        std::vector<SemanticType> parameters;
        for (const auto& mark : signature.parameters)
            parameters.push_back(typeOfMark(*mark));
        const std::optional<SemanticType> result = signature.returnType ? std::optional<SemanticType>(typeOfMark(*signature.returnType)) : std::nullopt;

        std::vector<const SubprogramInfo*> matches;
        for (const SubprogramInfo* candidate : overloads)
        {
            if (candidate->parameters.size() != parameters.size() || candidate->isFunction != result.has_value())
                continue;

            bool same = !result || candidate->returnType.info == result->info;
            for (size_t i = 0; same && i < parameters.size(); ++i)
                same = candidate->parameters[i].type.info == parameters[i].info;

            if (same)
                matches.push_back(candidate);
        }

        if (matches.empty())
            fail("No subprogram '" + (overloads.empty() ? std::string("?") : overloads.front()->name) + "' has this signature; the overloads are: "
                 + describeOverloads(overloads), at);

        return *matches.front();
    }

} // namespace Pulse::Parser
