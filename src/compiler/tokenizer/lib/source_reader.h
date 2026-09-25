#ifndef PULSE_PARSER_LIB_SOURCE_READER_H
#define PULSE_PARSER_LIB_SOURCE_READER_H

#include <cstddef>
#include <string>
#include <vector>
#include "tokenizer.h"

// Internal interface of the tokenizer. The public Tokenizer (include/tokenizer.h) owns the token vector and
// simply runs a SourceReader over the source once, at construction.
//
// The tokenizer is context-free: it follows the lexical rules of IEEE 1076-2008 (clause 15) and nothing else. It
// accepts every lexical element the standard allows, whether or not the rest of the pipeline can use it, and raises
// ast_lexical_error (with the position of the offending character) only for text that is not a valid token.
// The single piece of context it uses is the LRM's own rule for telling an attribute tick from a character literal
// (see tokenizeCharacterOrTick).

namespace Pulse::Parser
{
    /// A saved cursor position. Scanners take one at the start of a token and hand it to emit() / fail().
    struct SourceMark
    {
        size_t index;   ///< Byte offset.
        size_t line;    ///< 1-based line.
        size_t column;  ///< 1-based column.
    };

    /// Stateful cursor over a raw source string.
    /// Tracks the current byte index, line, and column, and emits tokens
    /// directly into the owning Tokenizer's token list.
    /// @note The scanning code is spread over several files by token family
    /// (source_reader.cc: cursor and trivia, words.cc, numbers.cc, strings.cc, operators.cc).
    class SourceReader
    {
        /// The complete source text (owned by the caller).
        const std::string& source;
        /// Destination of the emitted tokens.
        std::vector<Token>& out;
        /// Byte offset of the cursor.
        size_t index = 0;
        /// 1-based line of the cursor.
        size_t line = 1;
        /// 1-based column of the cursor.
        size_t column = 1;

        // ---- Navigation (source_reader.cc) ----
        /// Character under the cursor, or NUL at the end of the source.
        char current() const;
        /// Character `offset` positions ahead of the cursor, or NUL past the end.
        char peek(size_t offset = 1) const;
        /// True when the cursor is past the last character.
        bool atEnd() const;
        /// Moves the cursor forward, keeping line and column up to date. `\n`, `\r\n` and a lone `\r` each start a new line.
        void advance(size_t count = 1);
        /// The cursor position, to be used as the start of the token about to be scanned.
        SourceMark mark() const;
        /// Appends a token that started at `start` and ends at the cursor: call it after the token has been consumed.
        /// `text` is filled from the source, `value` is the normalized form chosen by the caller.
        void emit(TokenType type, std::string value, const SourceMark& start);

        /// Throws ast_lexical_error at the given position.
        [[noreturn]] void fail(const std::string& message, size_t atLine, size_t atColumn) const;
        [[noreturn]] void fail(const std::string& message, const SourceMark& at) const;

        /// Throws ast_lexical_error at the current position.
        [[noreturn]] void failHere(const std::string& message) const;

        // ---- Trivia (source_reader.cc) ----
        void skipTrivia();              ///< Skips whitespace, `--` and `/* */` comments.
        /// Skips the LRM separators: space, tab, VT, FF, CR, LF and the non-breaking space.
        void skipWhitespace();
        bool skipLineComment();         ///< Returns true if a comment was skipped.
        bool skipBlockComment();        ///< Returns true if a comment was skipped.

        /// Length in bytes of the graphic character starting `offset` bytes after the cursor, or 0 if there is none
        /// (end of source or a control character). Graphic characters are printable ASCII and non-ASCII characters,
        /// which are read as one UTF-8 sequence (a stray Latin-1 byte counts as one character of one byte).
        size_t graphicLength(size_t offset = 0) const;

        // ---- Words: identifiers, keywords, prefixed bit strings (words.cc) ----
        /// Basic identifiers, reserved words (classified through classifyWord) and prefixed bit strings like `x"FF"`.
        /// Basic identifiers are case-insensitive: the value is lowercased. No leading, trailing or doubled underscore.
        bool tokenizeWord();
        /// Rejects malformed basic identifiers with a message that points at the offending word.
        void validateIdentifier(const std::string& word, const SourceMark& start) const;

        // ---- Numbers and sized bit strings (numbers.cc) ----
        /// Decimal, real and based (`16#FF#`) numeric literals, and sized bit strings (`8x"FF"`). A number glued to letters
        /// (`10ns`) is an error: a separator is required before a unit.
        bool tokenizeNumber();
        /// Scans digits of `base` with single underscores between digits; returns them without underscores.
        /// For base 10 it stops at the first non-digit (the caller reports glued identifiers).
        std::string scanDigits(int base);
        /// Scans `e[+-]digits` when present and returns it (empty otherwise).
        std::string scanExponent();
        /// Emits the numeric token `text` after checking that no identifier character is glued to it.
        void finishNumber(const std::string& text, const SourceMark& start);
        /// `base#digits[.digits]#[exponent]`: bases 2..16 with digits validated against the base. The token keeps the
        /// `16#FF#` form; the parser evaluates it.
        void tokenizeBasedLiteral(const std::string& baseDigits, const SourceMark& start);
        /// `digits[.digits][exponent]`: emitted as a NumericLiteral with underscores removed.
        void tokenizeDecimalLiteral(std::string text, const SourceMark& start);
        /// Lookahead: does the number under the cursor continue as `<size>[u|s]<base>"..."`?
        bool startsSizedBitString() const;
        /// Sized bit string such as `8x"FF"` or `12sb"101"` (the size digits were already read): the prefix is validated and
        /// kept, lowercased, in the token value.
        void tokenizeSizedBitString(const std::string& sizeDigits, const SourceMark& start);

        // ---- Strings, bit strings, characters (strings.cc) ----
        /// String literal `"..."`; a doubled quote inside is kept as two quotes and un-escaped by the parser. Emitted as
        /// StringLiteral; the parser decides whether it is a bit string or a plain message.
        bool tokenizeString();
        /// Character literal `'x'` (emitted as CharacterLiteral including its quotes), or a lone `'` used as the attribute
        /// tick (`clk'event`, `t'('1')`, emitted as the Operator `'`).
        /// As in LRM 15.3, a `'` that follows an identifier, `)`, `]` or `all` is always a tick; anywhere else a quote,
        /// one graphic character and a quote form a character literal.
        bool tokenizeCharacterOrTick();
        /// Whether the last emitted token can be the prefix of an attribute name or qualified expression.
        bool previousTokenAllowsTick() const;
        /// Reads a double-quoted body (the cursor is on the opening quote) into `body`. A literal that reaches the end of
        /// the line or the source without a closing quote is an error located at its start.
        void scanQuotedBody(std::string& body, const SourceMark& start);
        /// Emits a bit-string token for `prefix` followed by the quoted body under the cursor.
        void emitBitString(const std::string& prefix, const SourceMark& start);

        // ---- Operators and delimiters (operators.cc) ----
        /// Multi-character operators, longest match first:
        /// `?/= ?<= ?>=` then `:= <= >= => /= ** <> << >> ?? ?= ?< ?>`.
        bool tokenizeCompoundOperator();
        /// Single-character delimiters: `( ) [ ] , ; : . @ ^` and the grave accent.
        bool tokenizeDelimiter();
        /// Single-character operators: `& + - * / < > = |`. The replacement character `!` is emitted as `|`.
        bool tokenizeSingleCharOperator();
        /// Reports a character that starts no token (e.g. `$`, `?` on its own).
        [[noreturn]] void failUnexpectedCharacter() const;

    public:
        /// Creates a reader over `source` that will append tokens to `out`.
        explicit SourceReader(const std::string& source, std::vector<Token>& out);

        /// Run the full tokenization loop. Returns the position just past the last character of the source.
        SourceMark tokenize();
    };
} // namespace Pulse::Parser

#endif // PULSE_PARSER_LIB_SOURCE_READER_H
