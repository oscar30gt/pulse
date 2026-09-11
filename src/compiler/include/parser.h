#ifndef PULSE_VHDL_PARSER_H
#define PULSE_VHDL_PARSER_H

#include "tokenizer.h"
#include "ast.h"

namespace Pulse::Parser
{
    /// Exception type thrown when an error occurs during AST construction.
    class ast_syntax_error : public std::runtime_error
    {
        SourceLocation m_location;

    public:
        ast_syntax_error(const std::string& message, const SourceLocation& location)
            : std::runtime_error(message), m_location(location) { }

        /// Location of the error inside the source file. Line and column numbers are 1-based.
        const SourceLocation& location() const { return m_location; }
    };

    // --------------------------------------------------------------------------------------------

    /// Takes a tokenized VHDL source file and generates its corresponding
    /// Abstract Syntax Tree (AST) representation.
    /// @param tokenizer A reference to a Tokenizer object that provides the tokenized VHDL source code.
    /// @returns An ASTRoot object representing the root of the generated AST.
    /// @note No semantic analysis is performed; the AST is purely syntactic.
    [[nodiscard]]
    ASTRoot VHDLtoAST(Tokenizer& tokenizer);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_PARSER_H