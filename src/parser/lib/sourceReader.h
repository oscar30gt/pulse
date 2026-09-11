#ifndef PULSE_VHDL_SOURCE_READER_H
#define PULSE_VHDL_SOURCE_READER_H

#include <cstddef>
#include <iterator>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <set>

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

// -------- Inline implementation -----------------------------------------------------------------

namespace Pulse::Parser
{
    namespace
    {
        /// Whether a character is a valid starting character for an identifier.
        /// @param c The character to check.
        /// @returns True if the character is a valid starting character for an identifier, false otherwise.
        bool isIdentifierStart(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
        }

        /// Whether a character is a valid character for an identifier (after the first character).
        /// @param c The character to check.
        /// @returns True if the character is a valid character for an identifier, false otherwise.
        bool isIdentifierChar(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        /// Lowers a string in place.
        /// @param s Reference to the string to lower.
        void toLowerInPlace(std::string& s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return std::tolower(c); }
            );
        }

        /// Whether the given lowercased identifier is a structural keyword.
        /// @param identifier Lowercased identifier to check.
        /// @returns True if the identifier is a keyword, false otherwise.
        bool isKeyword(const std::string& identifier)
        {
            static const std::set<std::string> keywords = {
                "use", "library", "entity", "architecture", "component",
                "is", "of", "begin", "end", "port", "map",
                "in", "out", "inout", "signal", "constant",
                "with", "select", "when", "else", "others", "open",
                "process", "if", "then", "elsif", "wait", "for"
            };

            return keywords.find(identifier) != keywords.end();
        }

        /// Whether the given lowercased identifier is a keyword operator (and, or, not, xor, etc.).
        /// These are syntactically keywords but are given TokenType::Operator
        /// to allow the renderer/parser to distinguish them from structural keywords.
        /// @param identifier Lowercased identifier to check.
        /// @returns True if it is a keyword operator.
        bool isOperatorKeyword(const std::string& identifier)
        {
            static const std::set<std::string> operatorKeywords = {
                "and", "or", "nand", "nor", "xor", "xnor",
                "not", "sll", "srl", "sla", "sra", "rol", "ror",
                "downto", "to",
            };

            return operatorKeywords.find(identifier) != operatorKeywords.end();
        }

        /// Classify a lowercased identifier into a TokenType.
        /// @param identifier Lowercased identifier just scanned.
        /// @returns The appropriate TokenType.
        TokenType classifyIdentifier(const std::string& identifier)
        {
            if (isOperatorKeyword(identifier))
                return TokenType::Operator;

            if (isKeyword(identifier))
                return TokenType::Keyword;

            return TokenType::Identifier;
        }

    } // anonymous namespace

    inline SourceReader::SourceReader(const std::string& source, std::vector<Token>& out)
        : source(source), out(out)
    { }

    inline char SourceReader::current() const
    {
        return source[index];
    }

    inline char SourceReader::peek(size_t offset) const
    {
        return source[index + offset];
    }

    inline bool SourceReader::atEnd() const
    {
        return index >= source.size();
    }

    inline bool SourceReader::atEnd(size_t offset) const
    {
        return index + offset >= source.size();
    }

    inline void SourceReader::advance(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (index >= source.size())
                continue;

            if (source[index] == '\n')
            {
                line++;
                column = 1;
            }
            else
            {
                column++;
            }
            index++;
        }
    }

    inline void SourceReader::emit(TokenType type, std::string value, size_t startLine, size_t startColumn)
    {
        out.push_back({ type, std::move(value), startLine, startColumn });
    }

    inline void SourceReader::skipWhitespace()
    {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(current())))
            advance();
    }

    inline bool SourceReader::skipLineComment()
    {
        if (atEnd(1) || current() != '-' || peek() != '-')
            return false;

        advance(2);
        while (!atEnd() && current() != '\n')
            advance();

        return true;
    }

    inline bool SourceReader::tokenizeIdentifierOrKeyword()
    {
        if (!isIdentifierStart(current()))
            return false;

        size_t startLine = line;
        size_t startColumn = column;
        size_t start  = index;

        advance();
        while (!atEnd() && isIdentifierChar(current()))
            advance();

        std::string identifier = source.substr(start, index - start);
        toLowerInPlace(identifier);

        // Bit string literal: prefix immediately followed by a quoted string (e.g. x"FF")
        if (!atEnd() && current() == '"')
        {
            static const std::set<std::string> bitStringPrefixes = {
                "b", "o", "x", "d",
                "ub", "uo", "ux",
                "sb", "so", "sx"
            };

            if (bitStringPrefixes.find(identifier) != bitStringPrefixes.end())
            {
                size_t strStart = index;
                advance(); // consume opening '"'

                while (!atEnd() && current() != '"')
                {
                    if (current() == '\n')
                        throw std::runtime_error("Tokenizer: unterminated bit string literal at line "
                        + std::to_string(line) + ", column " + std::to_string(column) + ".");
                    advance();
                }

                if (atEnd())
                    throw std::runtime_error("Tokenizer: unterminated bit string literal at line "
                    + std::to_string(line) + ", column " + std::to_string(column) + ".");

                advance(); // consume closing '"'
                emit(
                    TokenType::BitStringLiteral,
                    identifier + source.substr(strStart, index - strStart),
                    startLine,
                    startColumn
                );

                return true;
            }
        }

        TokenType type = classifyIdentifier(identifier);
        emit(type, std::move(identifier), startLine, startColumn);
        return true;
    }

    inline bool SourceReader::tokenizeNumericLiteral()
    {
        if (!std::isdigit(static_cast<unsigned char>(current())))
            return false;

        size_t startLine = line;
        size_t startColumn = column;
        size_t start = index;

        advance();
        while (!atEnd() && (
            std::isalnum(static_cast<unsigned char>(current())) ||      // Alnum characters (for hex, octal, etc.)
            current() == '_' || current() == '.' || current() == '#' || // Separators and decimal point
            ((current() == '+' || current() == '-') && (source[index - 1] == 'e' || source[index - 1] == 'E'))  // Sing when preceded by 'e' or 'E' for scientific notation
        ))
        {
            advance();
        }

        std::string number = source.substr(start, index - start);
        toLowerInPlace(number);

        // Sized bit string literal: e.g. 8x"FF"
        if (!atEnd() && current() == '"')
        {
            if (number.back() == 'b' || number.back() == 'o' || number.back() == 'x' || number.back() == 'd')
            {
                size_t strStart = index;
                advance(); // consume opening '"'

                while (!atEnd() && current() != '"')
                {
                    if (current() == '\n')
                        throw std::runtime_error("Tokenizer: unterminated sized bit string literal at line "
                        + std::to_string(line) + ", column " + std::to_string(column) + ".");
                    advance();
                }

                if (atEnd())
                    throw std::runtime_error("Tokenizer: unterminated sized bit string literal at line "
                    + std::to_string(line) + ", column " + std::to_string(column) + ".");

                advance(); // consume closing '"'
                emit(TokenType::BitStringLiteral,
                    number + source.substr(strStart, index - strStart),
                    startLine, startColumn);
                return true;
            }
        }

        emit(TokenType::NumericLiteral, std::move(number), startLine, startColumn);
        return true;
    }

    inline bool SourceReader::tokenizeBitStringLiteral()
    {
        if (current() != '"')
            return false;

        size_t startLine   = line;
        size_t startColumn = column;
        size_t start       = index;

        advance(); // consume opening '"'

        while (!atEnd() && current() != '"')
        {
            if (current() == '\n')
                throw std::runtime_error("Tokenizer: unterminated bit string literal at line "
                + std::to_string(line) + ", column " + std::to_string(column) + ".");
            advance();
        }

        if (atEnd())
            throw std::runtime_error("Tokenizer: unterminated bit string literal at line "
            + std::to_string(line) + ", column " + std::to_string(column) + ".");

        advance(); // consume closing '"'
        emit(TokenType::BitStringLiteral, source.substr(start, index - start), startLine, startColumn);
        return true;
    }

    inline bool SourceReader::tokenizeCharacterOrTick()
    {
        if (current() != '\'')
            return false;

        size_t startLine   = line;
        size_t startColumn = column;

        // Character literal: 'X' — exactly one character between the quotes
        if (!atEnd(2) && peek(2) == '\'')
        {
            emit(TokenType::CharacterLiteral, source.substr(index, 3), startLine, startColumn);
            advance(3);
        }
        else
        {
            // Attribute tick: e.g. sig'event
            emit(TokenType::Operator, "'", startLine, startColumn);
            advance();
        }
        return true;
    }

    inline bool SourceReader::tokenizeTwoCharOperator()
    {
        if (atEnd(1))
            return false;

        size_t startLine   = line;
        size_t startColumn = column;
        std::string two    = source.substr(index, 2);

        if (two == ":=" || two == "<=" || two == ">=" ||
            two == "=>" || two == "/=" || two == "**")
        {
            emit(TokenType::Operator, two, startLine, startColumn);
            advance(2);
            return true;
        }
        return false;
    }

    inline bool SourceReader::tokenizeDelimiter()
    {
        static const std::string delimiters = "()[],;:.";
        if (delimiters.find(current()) == std::string::npos)
            return false;

        emit(TokenType::Delimiter, std::string(1, current()), line, column);
        advance();
        return true;
    }

    inline bool SourceReader::tokenizeSingleCharOperator()
    {
        static const std::string operators = "&+-*/<>=|";
        if (operators.find(current()) == std::string::npos)
            return false;

        emit(TokenType::Operator, std::string(1, current()), line, column);
        advance();
        return true;
    }

    inline void SourceReader::tokenize()
    {
        while (!atEnd())
        {
            // Keep skipping whitespace and comments until we hit actual content
            bool madeProgress = true;
            while (madeProgress && !atEnd())
            {
                size_t before = index;
                skipWhitespace();
                if (!atEnd()) skipLineComment();
                madeProgress = (index > before);
            }

            if (atEnd())
                break;

            if (tokenizeIdentifierOrKeyword()) continue;
            if (tokenizeNumericLiteral()) continue;
            if (tokenizeBitStringLiteral()) continue;
            if (tokenizeCharacterOrTick()) continue;
            if (tokenizeTwoCharOperator()) continue;
            if (tokenizeDelimiter()) continue;
            if (tokenizeSingleCharOperator()) continue;

            throw std::runtime_error("Tokenizer: unexpected character '" + std::string(1, current()) +
                "' at line " + std::to_string(line) +
                ", column " + std::to_string(column) + ".");
        }
    }
} // namespace Pulse::Parser

#endif // PULSE_VHDL_SOURCE_READER_H