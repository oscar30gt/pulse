#ifndef PULSE_VHDL_AST_OPERATORS_H
#define PULSE_VHDL_AST_OPERATORS_H

#include <cstdint>

namespace Pulse::Parser
{
    /// Binary operators of VHDL-2008. `To` and `Downto` build ranges (`7 downto 0`) and are handled
    /// as operators of the left and right bound, as the tokenizer does.
    enum class BinaryOperator : uint8_t
    {
        // Logical
        And, Or, Nand, Nor, Xor, Xnor,
        // Relational
        Eq, Neq, Lt, Le, Gt, Ge,
        // Matching relational (`?=`, `?/=`, `?<`, `?<=`, `?>`, `?>=`)
        MatchEq, MatchNeq, MatchLt, MatchLe, MatchGt, MatchGe,
        // Shift and rotate
        Sll, Srl, Sla, Sra, Rol, Ror,
        // Arithmetic
        Add, Sub, Mul, Div, Mod, Rem, Pow,
        // Concatenation
        Concat,
        // Range direction
        To, Downto,
    };

    /// Unary operators of VHDL-2008. The `Reduce*` forms are the VHDL-2008 logical reductions (`and v`).
    enum class UnaryOperator : uint8_t
    {
        Plus, Minus, Abs, Not,
        Condition,                                       ///< `??`
        ReduceAnd, ReduceOr, ReduceNand, ReduceNor, ReduceXor, ReduceXnor,
    };

    /// Source spelling of an operator, lowercased ("+", "and", "downto", "?=", ...).
    const char* toString(BinaryOperator op);
    const char* toString(UnaryOperator op);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_OPERATORS_H
