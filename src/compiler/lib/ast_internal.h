#ifndef PULSE_VHDL_AST_INTERNAL_H
#define PULSE_VHDL_AST_INTERNAL_H

#include "ast.h"

namespace Pulse::Parser
{
    /// Clones an Expression object, creating a deep copy of the expression tree.
    /// @param src The source Expression to clone.
    /// @returns A unique pointer to the cloned Expression.
    ExpressionPtr cloneExpression(const Expression* src);

    /// Clones a TypeSpec object, creating a deep copy of the type specification.
    /// @param src The source TypeSpec to clone.
    /// @returns A new TypeSpec object that is a deep copy of the source.
    TypeSpec cloneTypeSpec(const TypeSpec& src);
    
} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_INTERNAL_H