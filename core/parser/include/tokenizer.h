#ifndef PULSE_VHDL_TOKENIZER_H
#define PULSE_VHDL_TOKENIZER_H

#include <vector>
#include <string>
#include <istream>
#include <cstdint>

namespace Pulse::Parser::VHDL
{
    /// Defines the broad lexical categories for VHDL tokens.
    /// This categorization avoids deep semantic analysis at the lexer stage.
    enum class TokenType : uint8_t
    {
        Identifier,
        Keyword,
        StandardType,
        StandardFunction,
        Attribute,
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
        TokenType type;
        std::string value;
        size_t line;
        size_t column;
    };

    /// Tokenizer utility class for parsing a single VHDL file into a sequence of tokens.
    /// During the tokenization process, the tokenizer will ignore comments and whitespace.
    /// No syntax checking is performed.
    /// @note Not all VHDL syntax is supported. Refer to the documentation 
    /// for a list of supported VHDL constructs.
    class Tokenizer
    {
        std::vector<Token> tokens;
        size_t currentIndex = 0;

        // Utility functions for tokenization. Used internally.
        void skipWhitespace(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        void skipLineComment(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        bool tokenizeIdentifierOrKeyword(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        bool tokenizeNumericLiteral(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        bool tokenizeStringLiteral(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        bool tokenizeCharacterOrTick(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        bool tokenizeTwoCharOperator(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        bool tokenizeDelimiter(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        bool tokenizeSingleCharOperator(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn);
        void advance(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn, size_t count = 1);

    public:
        Tokenizer(std::istream& input);
        ~Tokenizer();

        /// Size of the token stream.
        /// @returns The number of total tokens in the token stream.
        size_t size() const;

        /// Overloaded stream extraction operator to retrieve the next token from the stream.
        /// @returns true if a token was successfully extracted, false if the end of the stream has been reached. 
        bool operator>>(Token& token);
    };

} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_TOKENIZER_H