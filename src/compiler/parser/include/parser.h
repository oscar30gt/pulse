#ifndef PULSE_VHDL_PARSER_H
#define PULSE_VHDL_PARSER_H

#include "tokenizer.h"
#include "ast.h"

namespace Pulse::Parser
{
    /// Exception type thrown when the token stream does not follow the supported VHDL grammar.
    class ast_syntax_error : public compiler_error
    {
    public:
        ast_syntax_error(const std::string& message, const SourceLocation& location)
            : compiler_error(message, location) { }
    };

    // --------------------------------------------------------------------------------------------

    /// Takes a tokenized VHDL source file and generates its Abstract Syntax Tree (AST).
    ///
    /// The parser is blind: it checks the grammar (including which declarations and statements each region
    /// allows) but resolves no names, scopes or types. The AST is a hierarchical copy of the file; closing
    /// names (`end architecture rtl;`) are checked against the opening name and not stored.
    /// @param tokenizer The tokenized source. Parsing starts at its cursor.
    /// @returns The root of the AST, with one child per design unit in source order.
    /// @throws ast_syntax_error at the first token that does not fit the grammar.
    [[nodiscard]]
    ASTRoot VHDLtoAST(Tokenizer& tokenizer);

    /// Parses a standalone declarative part (types, subtypes, constants, subprograms ...) up to the end of
    /// the token stream, with the declarations an architecture allows. Used to build the predefined types.
    /// @throws ast_syntax_error at the first token that does not fit the grammar.
    [[nodiscard]]
    std::vector<DeclarationPtr> parseDeclarations(Tokenizer& tokenizer);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_PARSER_H
