#include "analyzer_internal.h"

namespace Pulse::Parser
{
    // ---- Every path returns ---------------------------------------------------------------------

    bool AnalyzerContext::exitsLoop(const std::vector<StatementPtr>& statements, const std::string& label, bool direct) const
    {
        for (const auto& statement : statements)
        {
            if (auto* exit = dynamic_cast<const ExitStatement*>(statement.get()))
            {
                // An unlabeled exit leaves the innermost loop; a labeled one leaves the loop it names.
                if (exit->loopLabel.empty() ? direct : (!label.empty() && exit->loopLabel == label))
                    return true;
            }
            else if (auto* branching = dynamic_cast<const IfStatement*>(statement.get()))
            {
                for (const auto& branch : branching->branches)
                    if (exitsLoop(branch->body, label, direct))
                        return true;
                if (exitsLoop(branching->elseBody, label, direct))
                    return true;
            }
            else if (auto* selection = dynamic_cast<const CaseStatement*>(statement.get()))
            {
                for (const auto& alternative : selection->alternatives)
                    if (exitsLoop(alternative->body, label, direct))
                        return true;
            }
            else if (auto* loop = dynamic_cast<const LoopStatement*>(statement.get()))
            {
                if (exitsLoop(loop->body, label, false))
                    return true;
            }
            else if (auto* whileLoop = dynamic_cast<const WhileLoopStatement*>(statement.get()))
            {
                if (exitsLoop(whileLoop->body, label, false))
                    return true;
            }
            else if (auto* forLoop = dynamic_cast<const ForLoopStatement*>(statement.get()))
            {
                if (exitsLoop(forLoop->body, label, false))
                    return true;
            }
        }
        return false;
    }

    /// True when control cannot get past the end of `statements` without meeting a `return`.
    bool AnalyzerContext::alwaysReturns(const std::vector<StatementPtr>& statements) const
    {
        for (const auto& statement : statements)
        {
            if (dynamic_cast<const ReturnStatement*>(statement.get()))
                return true;

            if (auto* branching = dynamic_cast<const IfStatement*>(statement.get()))
            {
                bool all = !branching->elseBody.empty() && alwaysReturns(branching->elseBody);
                for (const auto& branch : branching->branches)
                    all = all && alwaysReturns(branch->body);
                if (all)
                    return true;
            }
            else if (auto* selection = dynamic_cast<const CaseStatement*>(statement.get()))
            {
                // The choices of a case cover every value (or end in `others`), so one of the alternatives always runs.
                bool all = !selection->alternatives.empty();
                for (const auto& alternative : selection->alternatives)
                    all = all && alwaysReturns(alternative->body);
                if (all)
                    return true;
            }
            else if (auto* loop = dynamic_cast<const LoopStatement*>(statement.get()))
            {
                // A loop without exit never ends, so nothing after it can be reached.
                if (!exitsLoop(loop->body, loop->label, true))
                    return true;
            }
        }
        return false;
    }

    // ---- Calls that wait ------------------------------------------------------------------------

    /// Everything a body calls is known once the region is analyzed: a procedure waits when it, or a procedure it calls, does.
    void AnalyzerContext::checkCallGraph(size_t firstSubprogram)
    {
        for (bool changed = true; changed;)
        {
            changed = false;
            for (size_t i = firstSubprogram; i < m_subprograms.size(); ++i)
            {
                SubprogramInfo& info = *m_subprograms[i];
                if (info.containsWait)
                    continue;

                for (const SubprogramInfo* callee : info.callees)
                {
                    if (callee->containsWait)
                    {
                        info.containsWait = true;
                        changed = true;
                        break;
                    }
                }
            }
        }

        for (size_t i = firstSubprogram; i < m_subprograms.size(); ++i)
        {
            const SubprogramInfo& info = *m_subprograms[i];
            if (!info.isFunction)
                continue;

            for (size_t call = 0; call < info.callees.size(); ++call)
            {
                const SubprogramInfo& callee = *info.callees[call];
                if (callee.containsWait)
                    fail("The function '" + info.name + "' cannot call the procedure '" + callee.name + "', which contains a wait statement",
                         info.calleeLocations[call]);
            }
        }

        // A concurrent procedure call may wait: its equivalent process has no sensitivity list, only a final wait statement
        // (LRM 11.4), as in the clock generator `clk_gen(clk, 10 ns);`.
        for (const StatementCall& call : m_statementCalls)
        {
            if (!call.callee->containsWait || !call.process)
                continue;

            if (!call.process->sensitivityList.empty() || call.process->sensitivityAll)
                fail("The process has a sensitivity list, so it cannot call '" + call.callee->name + "', which contains a wait statement", call.location);
        }
    }

} // namespace Pulse::Parser
