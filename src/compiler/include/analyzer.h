#ifndef PULSE_VHDL_ANALYZER_H
#define PULSE_VHDL_ANALYZER_H

#include "ast.h"

namespace Pulse::Parser
{
    /// Exception type thrown when an error occurs during semantic analysis of the AST.
    class ast_semantic_error : public std::runtime_error
    {
        SourceLocation m_location;

    public:
        ast_semantic_error(const std::string& message, const SourceLocation& location)
            : std::runtime_error(message), m_location(location) { }

        /// Location of the error inside the source file. Line and column numbers are 1-based.
        const SourceLocation& location() const { return m_location; }
    };

    // --------------------------------------------------------------------------------------------

    /// Given an AST root, analyze and validate the AST for semantic correctness.
    /// This includes type checking, scope resolution and any other errors that
    /// were not caught during the AST construction phase.
    /// @param root The root of the AST to analyze. Read-only; the AST is not modified during analysis.
    /// @throws ast_semantic_error if any semantic errors are found during analysis.
    [[nodiscard]]
    void analyzeAST(const ASTRoot& root);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_ANALYZER_H