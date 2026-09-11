#include "tokenizer_internal.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <string>

namespace Pulse::Parser
{
    namespace
    {
        /// Whether a character is a valid starting character for an identifier.
        bool isIdentifierStart(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
        }

        /// Whether a character is a valid non-initial character for an identifier.
        bool isIdentifierChar(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        /// Lowers a string in place.
        void toLowerInPlace(std::string& s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return std::tolower(c); }
            );
        }

        /// Whether the given lowercased identifier is a structural keyword.
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

        /// Whether the given lowercased identifier is a keyword operator
        /// (and, or, not, xor, …).  These are syntactically keywords but
        /// receive TokenType::Operator so the parser can distinguish them from
        /// structural keywords.
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
        TokenType classifyIdentifier(const std::string& identifier)
        {
            if (isOperatorKeyword(identifier))
                return TokenType::Operator;

            if (isKeyword(identifier))
                return TokenType::Keyword;

            return TokenType::Identifier;
        }

        /// The set of unsized bit-string prefixes that may immediately precede
        /// a quoted body (e.g. x"FF", sb"0101").
        const std::set<std::string>& unsizedBitStringPrefixes()
        {
            static const std::set<std::string> prefixes = {
                "b", "o", "x", "d",
                "ub", "uo", "ux", "ud",
                "sb", "so", "sx", "sd"
            };
            return prefixes;
        }

    } // anonymous namespace

    // ---- Identifiers and keywords -------------------------------------------

    bool SourceReader::tokenizeIdentifierOrKeyword()
    {
        if (!isIdentifierStart(current()))
            return false;

        size_t startLine   = line;
        size_t startColumn = column;
        size_t start       = index;

        advance();
        while (!atEnd() && isIdentifierChar(current()))
            advance();

        std::string identifier = source.substr(start, index - start);
        toLowerInPlace(identifier);

        // Unsized bit-string literal: prefix immediately followed by a quoted
        // body — e.g. x"FF", sb"0101", ux"A0".
        if (!atEnd() && current() == '"')
        {
            if (unsizedBitStringPrefixes().count(identifier))
            {
                size_t strStart = index;
                advance(); // consume opening '"'

                while (!atEnd() && current() != '"')
                {
                    if (current() == '\n')
                        throw std::runtime_error(
                            "Tokenizer: unterminated bit-string literal at line "
                            + std::to_string(line) + ", column " + std::to_string(column) + ".");
                    advance();
                }

                if (atEnd())
                    throw std::runtime_error(
                        "Tokenizer: unterminated bit-string literal at line "
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

    // ---- Two-character operators --------------------------------------------

    bool SourceReader::tokenizeTwoCharOperator()
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

    // ---- Delimiters ---------------------------------------------------------

    bool SourceReader::tokenizeDelimiter()
    {
        static const std::string delimiters = "()[],;:.";
        if (delimiters.find(current()) == std::string::npos)
            return false;

        emit(TokenType::Delimiter, std::string(1, current()), line, column);
        advance();
        return true;
    }

    // ---- Single-character operators -----------------------------------------

    bool SourceReader::tokenizeSingleCharOperator()
    {
        static const std::string operators = "&+-*/<>=|";
        if (operators.find(current()) == std::string::npos)
            return false;

        emit(TokenType::Operator, std::string(1, current()), line, column);
        advance();
        return true;
    }

    // ---- Main tokenization loop ---------------------------------------------

    void SourceReader::tokenize()
    {
        while (!atEnd())
        {
            // Keep skipping whitespace and comments until we reach actual content.
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
            if (tokenizeNumericLiteral())      continue;
            if (tokenizeBitStringLiteral())    continue;
            if (tokenizeCharacterOrTick())     continue;
            if (tokenizeTwoCharOperator())     continue;
            if (tokenizeDelimiter())           continue;
            if (tokenizeSingleCharOperator())  continue;

            throw std::runtime_error(
                "Tokenizer: unexpected character '" + std::string(1, current()) +
                "' at line " + std::to_string(line) +
                ", column " + std::to_string(column) + ".");
        }
    }

} // namespace Pulse::Parser
