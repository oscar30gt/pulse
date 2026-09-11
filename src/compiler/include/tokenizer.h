#ifndef PULSE_VHDL_TOKENIZER_H
#define PULSE_VHDL_TOKENIZER_H

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
        BitStringLiteral,
        CharacterLiteral,
        Operator,
        Delimiter,
        Unknown,
    };

    /// Represents a single lexical token extracted from the source file.
    /// Contains positional tracking for accurate diagnostic reporting.
    struct Token
    {
        TokenType type;     ///< Type of the token
        std::string value;  ///< The actual string value of the token
        size_t line;        ///< Line number in the source file where the token was found (1-based)
        size_t column;      ///< Column number in the source file where the token starts (1-based)
    };
    
    /// Tokenizer utility class for parsing a single VHDL file into a sequence of tokens.
    /// During the tokenization process, the tokenizer will ignore comments and whitespace.
    /// No syntax checking is performed.
    /// @note Not all VHDL syntax is supported. Refer to the documentation
    /// for a list of supported VHDL constructs.
    class Tokenizer
    {
        std::vector<Token> m_tokens;
        size_t m_currentIndex = 0;

    public:
        Tokenizer(std::istream& input);
        Tokenizer(const std::string& input);
        ~Tokenizer();

        /// Get the current cursor position.
        /// @returns Cursor index, where 0 is the first token, and size() is the end of the stream.
        size_t index() const;

        /// Get the total number of tokens.
        /// @returns Number of tokens, including those that have already been consumed.
        size_t size() const;

        /// Get the number of tokens remaining, from the current cursor position to the end.
        /// @returns Number of tokens remaining.
        size_t remaining() const;

        /// Advance the cursor by `count` tokens and return the token now under the cursor.
        /// @param count Number of tokens to advance. Defaults to 1.
        /// @returns Pointer to the token now under the cursor, or nullptr if past the end.
        const Token* next(size_t count = 1);

        /// Move the cursor back by `count` tokens and return the token now under the cursor.
        /// @param count Number of tokens to move back. Defaults to 1.
        /// @returns Pointer to the token now under the cursor, or nullptr if before the beginning.
        const Token* prev(size_t count = 1);

        /// Peek at a token at a specific offset from the current cursor position without moving the cursor.
        /// @param offset The offset from the current cursor position. Positive values look ahead, negative values look behind.
        /// @returns Pointer to the token at the specified offset from the current cursor position, or nullptr if out of bounds.
        [[nodiscard]]
        const Token* peek(ssize_t offset = 0) const;

        /// Whether the cursor has reached the end of the token vector.
        /// @returns `true` if the cursor is at the end and no more tokens are available for consumption, `false` otherwise.
        bool end() const;
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_TOKENIZER_H