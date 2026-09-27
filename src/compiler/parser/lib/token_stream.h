#ifndef PULSE_PARSER_LIB_TOKEN_STREAM_H
#define PULSE_PARSER_LIB_TOKEN_STREAM_H

#include "parser.h"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

namespace Pulse::Parser
{
    /// The parser's cursor over the tokens of one file. It is the only code that talks to the Tokenizer.
    ///
    /// Reserved words, operators and delimiters are matched by `at` / `accept` / `expect`, which compare
    /// the token type as well as its value: an identifier or a literal can never match a reserved word.
    /// Past the last token every lookup sees the end-of-file token, so no call ever returns null.
    class TokenStream
    {
        Tokenizer& m_tokenizer;

    public:
        explicit TokenStream(Tokenizer& tokenizer);

        // ---- Looking ----------------------------------------------------------------------------

        /// The token `offset` positions ahead of the cursor (0 is the current one), or the end-of-file token.
        const Token* peek(size_t offset = 0) const;
        size_t position() const;                                            ///< Cursor index, for seek().
        bool atEnd() const;                                                 ///< The cursor is on the end-of-file token.
        bool at(std::string_view word, size_t offset = 0) const;            ///< Reserved word, operator or delimiter `word`.
        bool atAny(std::initializer_list<std::string_view> words) const;    ///< at() for any of `words`.
        bool atType(TokenType type, size_t offset = 0) const;

        // ---- Consuming --------------------------------------------------------------------------

        const Token* next();                                                ///< Consumes and returns the current token.
        const Token* accept(std::string_view word);                         ///< Consumes `word` if present, else null.
        const Token* expect(std::string_view word);                         ///< Consumes `word`, or fails.
        const Token* expectIdentifier();                                    ///< Consumes an identifier, or fails.
        void seek(size_t position);                                         ///< Moves the cursor back (bounded backtracking).

        // ---- Errors -----------------------------------------------------------------------------

        /// "Expected <what>, but found <current token>", at the current token.
        [[noreturn]] void expected(std::string_view what) const;
        /// Fails with `message` at the current token.
        [[noreturn]] void fail(const std::string& message) const;
        /// Fails with `message` at `token`.
        [[noreturn]] void failAt(const Token* token, const std::string& message) const;
    };

    /// A token as it reads in a message: `'foo'` (as written), or "the end of the file".
    std::string describe(const Token* token);

    /// Location of a token, for AST nodes and errors.
    SourceLocation locationOf(const Token* token);

} // namespace Pulse::Parser

#endif // PULSE_PARSER_LIB_TOKEN_STREAM_H
