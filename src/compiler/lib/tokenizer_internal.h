#ifndef PULSE_PARSER_LIB_TOKENIZER_H
#define PULSE_PARSER_LIB_TOKENIZER_H

#include <cstddef>
#include <string>
#include <vector>
#include "tokenizer.h"

namespace Pulse::Parser
{
    /// Stateful cursor over a raw source string.
    /// Tracks the current byte index, line, and column, and emits tokens
    /// directly into the owning Tokenizer's token list.
    /// @note Declared as a friend of Tokenizer so it can push tokens without
    /// exposing that list publicly.
    class SourceReader
    {
        const std::string& source;
        std::vector<Token>& out;
        size_t index = 0;
        size_t line = 1;
        size_t column = 1;

        // Low-level navigation
        char current() const;
        char peek(size_t offset = 1) const;
        bool atEnd() const;
        bool atEnd(size_t offset) const;
        void advance(size_t count = 1);

        // Skip helpers
        void skipWhitespace();
        bool skipLineComment();   ///< Returns true if a comment was skipped.

        // Tokenize helpers: return true on success, false if the character
        // at the current position does not match this token type.
        bool tokenizeIdentifierOrKeyword();
        bool tokenizeNumericLiteral();
        bool tokenizeBitStringLiteral();
        bool tokenizeCharacterOrTick();
        bool tokenizeTwoCharOperator();
        bool tokenizeDelimiter();
        bool tokenizeSingleCharOperator();

        // Push a completed token into the output list.
        void emit(TokenType type, std::string value, size_t startLine, size_t startColumn);

    public:
        explicit SourceReader(const std::string& source, std::vector<Token>& out);

        /// Run the full tokenization loop.
        void tokenize();
    };
} // namespace Pulse::Parser

#endif // PULSE_PARSER_LIB_TOKENIZER_H