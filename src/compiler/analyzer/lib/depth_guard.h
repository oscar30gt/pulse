#ifndef PULSE_COMPILER_ANALYZER_DEPTH_GUARD_H
#define PULSE_COMPILER_ANALYZER_DEPTH_GUARD_H

#include <cstddef>

namespace Pulse::Parser
{
    /// Deepest nesting of one expression the analyzer walks. The analysis recurses along the tree, so hostile input (thousands
    /// of chained operators) must fail cleanly instead of overflowing the stack.
    constexpr size_t maxExpressionDepth = 250;

    /// Counts the depth of the expression being walked for as long as the object lives.
    class DepthGuard
    {
        size_t& m_depth;
    public:
        explicit DepthGuard(size_t& depth) : m_depth(depth) { ++m_depth; }
        ~DepthGuard() { --m_depth; }
        DepthGuard(const DepthGuard&) = delete;
        DepthGuard& operator=(const DepthGuard&) = delete;
    };

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_ANALYZER_DEPTH_GUARD_H
