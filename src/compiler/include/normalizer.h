#ifndef PULSE_VHDL_NORMALIZER_H
#define PULSE_VHDL_NORMALIZER_H

#include "ast.h"

namespace Pulse::Parser
{
    /// Normalizes the given AST root by applying transformations and simplifications
    /// to ensure a consistent and optimized representation of the design.
    /// Normalization examples:
    /// - Normalize ranges to (X downto 0) format.
    /// - Homogeneous conditional expressions (e.g., with-select converted to when-else).
    /// - Logic-type conversions (e.g., boolean expressions to std_logic and integers to std_logic_vector(31 downto 0)).
    /// @param root The AST root to be normalized in place.
    void normalizeAST(ASTRoot& root);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_NORMALIZER_H