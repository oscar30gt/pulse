#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    // ---- Diagnostics text -----------------------------------------------------------------------

    std::string AnalyzerContext::describeProfile(const SubprogramInfo& info) const
    {
        std::string text = info.name + "(";
        for (size_t i = 0; i < info.parameters.size(); ++i)
            text += (i ? ", " : "") + info.parameters[i].type.info->name;
        text += ")";

        if (info.isFunction)
            text += " return " + describe(info.returnType);
        return text;
    }

    std::string AnalyzerContext::describeOverloads(const std::vector<const SubprogramInfo*>& overloads) const
    {
        std::string text;
        for (const SubprogramInfo* overload : overloads)
            text += (text.empty() ? "" : "; ") + describeProfile(*overload);
        return text;
    }

    // ---- Trying a candidate ---------------------------------------------------------------------

    /// Why a call with these arguments does not fit `candidate`; empty when it fits. Nothing is thrown or recorded.
    std::string AnalyzerContext::candidateProblem(const SubprogramInfo& candidate, const std::vector<const Expression*>& arguments,
                                                  const ASTNode& at)
    {
        try
        {
            const std::string owner = "'" + candidate.name + "'";
            AssociationResult result = matchAssociations(candidate.parameters, arguments, AssociationKind::Call, owner, at);
            if (!result.ok())
                return result.problem;

            for (const Binding& binding : result.bindings)
                checkBinding(binding, candidate.parameters[binding.formal], AssociationKind::Call,
                             "parameter '" + candidate.parameters[binding.formal].name + "' of " + owner);
        }
        catch (const ast_semantic_error& error)
        {
            return error.what();
        }
        return "";
    }

    // ---- Committing a call ----------------------------------------------------------------------

    void AnalyzerContext::noteCall(const SubprogramInfo& callee, const ASTNode& at)
    {
        if (m_subprogram)
        {
            m_subprogram->callees.push_back(&callee);
            m_subprogram->calleeLocations.push_back(at.source);
        }
        else
        {
            m_statementCalls.push_back({ m_process, &callee, at.source });
        }
    }

    /// The chosen overload is checked for real (so the errors are the ones of that call) and the call is recorded.
    void AnalyzerContext::commitCall(const SubprogramInfo& callee, const ASTNode& node, const std::vector<const Expression*>& arguments)
    {
        const std::string owner = "'" + callee.name + "'";
        AssociationResult result = matchAssociations(callee.parameters, arguments, AssociationKind::Call, owner, node);
        if (!result.ok())
            fail(result.problem, *result.at);

        for (const Binding& binding : result.bindings)
        {
            const FormalInfo& formal = callee.parameters[binding.formal];
            checkBinding(binding, formal, AssociationKind::Call, "parameter '" + formal.name + "' of " + owner);
        }

        // A signal the call may assign becomes a driver of the caller's source.
        for (const Binding& binding : result.bindings)
        {
            const FormalInfo& formal = callee.parameters[binding.formal];
            if (formal.kind == SymbolKind::Signal && formal.mode != PortMode::In && !dynamic_cast<const OpenExpr*>(binding.actual))
                recordDriver(*binding.actual, node);
        }

        if (m_subprogram && m_subprogram->isFunction && !m_subprogram->impure && callee.isFunction && callee.impure)
            fail("The pure function '" + m_subprogram->name + "' cannot call the impure function '" + callee.name + "'", node);

        noteCall(callee, node);
        m_resolvedCalls[&node] = &callee;
    }

    // ---- Overload resolution --------------------------------------------------------------------

    const SubprogramInfo& AnalyzerContext::resolveCall(const std::string& name, const ASTNode& node, const std::vector<const Expression*>& arguments,
                                                       const SemanticType* expected, bool wantFunction)
    {
        std::vector<const SubprogramInfo*> candidates;
        for (const SubprogramInfo* overload : visibleSubprograms(name))
            if (overload->isFunction == wantFunction)
                candidates.push_back(overload);

        if (candidates.empty())
            fail(wantFunction ? "'" + name + "' is a procedure, and a procedure call is not a value"
                              : "'" + name + "' is a function, and a function can only be called inside an expression", node);

        // One candidate: check the call against it directly, so the diagnostics are the ones of that call.
        if (candidates.size() == 1)
        {
            commitCall(*candidates.front(), node, arguments);
            return *candidates.front();
        }

        std::vector<const SubprogramInfo*> fitting;
        std::string reasons;
        for (const SubprogramInfo* candidate : candidates)
        {
            const std::string problem = candidateProblem(*candidate, arguments, node);
            if (problem.empty())
                fitting.push_back(candidate);
            else
                reasons += "\n  " + describeProfile(*candidate) + ": " + problem;
        }

        if (fitting.empty())
            fail("No overload of '" + name + "' accepts these arguments. Candidates:" + reasons, node);

        // Several accept the arguments: the type the context expects may pick one of them.
        if (fitting.size() > 1 && wantFunction && expected && expected->valid())
        {
            std::vector<const SubprogramInfo*> byResult;
            for (const SubprogramInfo* candidate : fitting)
                if (candidate->returnType.info == expected->info)
                    byResult.push_back(candidate);

            if (!byResult.empty())
                fitting = std::move(byResult);
        }

        if (fitting.size() > 1)
            fail("The call of '" + name + "' is ambiguous; it fits " + describeOverloads(fitting), node);

        commitCall(*fitting.front(), node, arguments);
        return *fitting.front();
    }

    SemanticType AnalyzerContext::typeOfFunctionCall(const std::string& name, const ASTNode& node, const std::vector<const Expression*>& arguments,
                                                     const SemanticType* expected)
    {
        return resolveCall(name, node, arguments, expected, true).returnType;
    }

} // namespace Pulse::Parser
