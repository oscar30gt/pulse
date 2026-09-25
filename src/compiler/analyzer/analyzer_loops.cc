#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    /// Enters a loop; labels must be unique among the enclosing loops. (The label itself is declared with the statement.)
    void AnalyzerContext::enterLoop(const std::string& label, const ASTNode& node)
    {
        if (!label.empty() && std::find(m_loops.begin(), m_loops.end(), label) != m_loops.end())
            fail("The loop label '" + label + "' is already used by an enclosing loop", node);

        m_loops.push_back(label);
    }

    void AnalyzerContext::analyzeForLoop(const ForLoopStatement& loop)
    {
        const RangeInfo range = analyzeRange(*loop.range);
        enterLoop(loop.label, loop);

        // The loop parameter is a constant of the range type (`integer` for a range of literals).
        Symbol parameter;
        parameter.kind = SymbolKind::LoopParameter;
        parameter.type = isUniversalInteger(range.type) ? typeOf(*m_std.integer) : range.type;
        parameter.objectId = newObjectId();

        pushScope();
        declare(loop.parameter, std::move(parameter), loop);
        analyzeSequence(loop.body);
        popScope();

        m_loops.pop_back();
    }

    void AnalyzerContext::analyzeWhileLoop(const WhileLoopStatement& loop)
    {
        requireCondition(*loop.condition, "the 'while' loop");
        enterLoop(loop.label, loop);
        analyzeSequence(loop.body);
        m_loops.pop_back();
    }

    void AnalyzerContext::analyzeLoop(const LoopStatement& loop)
    {
        enterLoop(loop.label, loop);
        analyzeSequence(loop.body);
        m_loops.pop_back();
    }

    // ---- exit / next ----------------------------------------------------------------------------

    void AnalyzerContext::checkLoopControl(const std::string& keyword, const std::string& label, const Expression* condition,
                                           const ASTNode& node)
    {
        if (m_loops.empty())
            fail("'" + keyword + "' can only be used inside a loop", node);

        if (!label.empty() && std::find(m_loops.begin(), m_loops.end(), label) == m_loops.end())
            fail("There is no enclosing loop labeled '" + label + "' for '" + keyword + "'", node);

        if (condition)
            requireCondition(*condition, "the '" + keyword + " when' clause");
    }

    void AnalyzerContext::analyzeExit(const ExitStatement& statement)
    {
        checkLoopControl("exit", statement.loopLabel, statement.condition.get(), statement);
    }

    void AnalyzerContext::analyzeNext(const NextStatement& statement)
    {
        checkLoopControl("next", statement.loopLabel, statement.condition.get(), statement);
    }

} // namespace Pulse::Parser
