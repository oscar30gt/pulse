#ifndef PULSE_VHDL_TOKENIZER_H
#define PULSE_VHDL_TOKENIZER_H

#include "diagnostics.h"

#include <vector>
#include <string>
#include <istream>
#include <cstdint>
#include <cstddef>

#if defined(_MSC_VER) && !defined(__clang__)
    #define ssize_t __int64
#endif

namespace Pulse::Parser
{
    /// Defines the broad lexical categories for VHDL tokens.
    /// This categorization avoids deep semantic analysis at the lexer stage.
    enum class TokenType : uint8_t
    {
        Identifier,
        Keyword,
        NumericLiteral,
        StringLiteral,
        CharacterLiteral,
        Operator,
        Delimiter,
        EndOfFile,
    };

    /// Represents a single lexical token extracted from the source file.
    /// Contains positional tracking for accurate diagnostic reporting.
    struct Token
    {
        TokenType type;         ///< Type of the token
        std::string value;      ///< Normalized value: words lowercased, number underscores removed, `!` written as `|`.
        std::string original;   ///< The token exactly as written in the source (case preserved)
        size_t line;            ///< Line where the token starts (1-based)
        size_t column;          ///< Column where the token starts (1-based, counted in bytes)
        size_t file = noFile;   ///< Index of the source file, as given to the Tokenizer
    };
    
    /// Splits a single VHDL file into tokens and lets the caller walk through them with a cursor.
    ///
    /// The tokenizer follows the lexical rules of VHDL-2008 (IEEE 1076-2008, clause 15): every valid lexical element
    /// except extended identifiers (`\name\`, rejected) is accepted whether or not the rest of the pipeline supports it,
    /// and comments and whitespace are dropped.
    /// It has no notion of syntax or meaning; the constructor throws `ast_lexical_error` only for text that is not a
    /// valid token (malformed numbers, unterminated strings, stray characters, ...).
    class Tokenizer
    {
        std::vector<Token> m_tokens;
        size_t m_currentIndex = 0;
        Token m_endOfFile;

    public:
        /// Tokenizes a whole source.
        /// @param input The VHDL text.
        /// @param file Index of the source file, stamped on every token and lexical error so diagnostics can name the file.
        Tokenizer(std::istream& input, size_t file = noFile);
        Tokenizer(const std::string& input, size_t file = noFile);
        ~Tokenizer();

        /// Get the current cursor position.
        /// @returns Cursor index, where 0 is the first token, and size() is the end of the stream.
        size_t index() const;

        /// Move the cursor to an absolute position (for backtracking). Values past the end are clamped to size().
        /// @param position Cursor index, as returned by index().
        void seek(size_t position);

        /// Get the total number of tokens.
        /// @returns Number of tokens, including those that have already been consumed.
        size_t size() const;

        /// Get the number of tokens remaining, from the current cursor position to the end.
        /// @returns Number of tokens remaining.
        size_t remaining() const;

        /// The end-of-input sentinel token (type `TokenType::EndOfFile`), located just after the last real token.
        /// It is not counted by size() and never stored in the token sequence.
        const Token* eof() const;

        /// Advance the cursor by `count` tokens and return the last token that was consumed.
        /// @param count Number of tokens to advance. Defaults to 1.
        /// @returns Pointer to the last consumed token, or the end-of-file token if `count` exceeds the remaining tokens.
        const Token* next(size_t count = 1);

        /// Move the cursor back by `count` tokens and return the token now under the cursor.
        /// @param count Number of tokens to move back. Defaults to 1.
        /// @returns Pointer to the token now under the cursor, or nullptr if before the beginning.
        const Token* prev(size_t count = 1);

        /// Peek at a token at a specific offset from the current cursor position without moving the cursor.
        /// @param offset The offset from the current cursor position. Positive values look ahead, negative values look behind.
        /// @returns Pointer to the token at the specified offset, the end-of-file token if the offset is past the last token,
        /// or nullptr if it is before the first token.
        [[nodiscard]]
        const Token* peek(ssize_t offset = 0) const;

        /// Whether the cursor has reached the end of the token vector.
        /// @returns `true` if the cursor is at the end and no more tokens are available for consumption, `false` otherwise.
        bool end() const;
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_TOKENIZER_H