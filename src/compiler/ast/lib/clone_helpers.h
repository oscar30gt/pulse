#ifndef PULSE_VHDL_AST_CLONE_HELPERS_H
#define PULSE_VHDL_AST_CLONE_HELPERS_H

#include "ast.h"

#include <type_traits>

namespace Pulse::Parser
{
    /// Starts a copy of `src`: a default-constructed node of the same type with the data every node shares
    /// (source location, the label of a statement and the context clause of a design unit). The caller fills
    /// in the node's own members.
    template <typename T>
    std::unique_ptr<T> copyShell(const T& src)
    {
        auto copy = std::make_unique<T>();
        copy->source = src.source;
        if constexpr (std::is_base_of_v<Statement, T>)
            copy->label = src.label;
        if constexpr (std::is_base_of_v<DesignUnit, T>)
            copy->context = cloneAll(src.context);
        return copy;
    }

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_CLONE_HELPERS_H
