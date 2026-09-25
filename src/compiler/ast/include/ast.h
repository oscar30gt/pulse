#ifndef PULSE_VHDL_AST_H
#define PULSE_VHDL_AST_H

// Umbrella header of the AST: the node base classes (node.h) and every concrete node.

#include "node.h"
#include "operators.h"
#include "expressions.h"
#include "statements.h"
#include "declarations.h"

namespace Pulse::Parser
{
    /// Root node containing the design units (entities and architectures) of a design.
    struct ASTRoot final : ASTNode
    {
        /// Top-level entities and architectures in the order they were parsed.
        std::vector<DesignUnitPtr> children;

        PULSE_AST_NODE_OVERRIDES
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_H
