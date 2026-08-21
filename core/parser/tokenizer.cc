#include "tokenizer.h"

#include <istream>
#include <iterator>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>

namespace Pulse::Parser::VHDL
{
    namespace
    {
        bool isIdentifierStart(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
        }

        bool isIdentifierChar(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        void toLowerInPlace(std::string& s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return std::tolower(c); });
        }

        TokenType classifyIdentifier(const std::string& identifier)
        {
            static const std::unordered_map<std::string, TokenType> keywordMap = {
                {"entity", TokenType::Keyword}, {"architecture", TokenType::Keyword},
                {"is", TokenType::Keyword}, {"begin", TokenType::Keyword},
                {"end", TokenType::Keyword}, {"port", TokenType::Keyword},
                {"map", TokenType::Keyword}, {"signal", TokenType::Keyword},
                {"constant", TokenType::Keyword}, {"generic", TokenType::Keyword},
                {"process", TokenType::Keyword}, {"if", TokenType::Keyword},
                {"then", TokenType::Keyword}, {"else", TokenType::Keyword},
                {"elsif", TokenType::Keyword}, {"case", TokenType::Keyword},
                {"when", TokenType::Keyword}, {"with", TokenType::Keyword},
                {"select", TokenType::Keyword}, {"for", TokenType::Keyword},
                {"in", TokenType::Keyword}, {"out", TokenType::Keyword},
                {"downto", TokenType::Keyword}, {"to", TokenType::Keyword},
                {"open", TokenType::Keyword}, {"component", TokenType::Keyword},
                {"use", TokenType::Keyword}, {"library", TokenType::Keyword},
                {"others", TokenType::Keyword}, {"all", TokenType::Keyword},
                {"of", TokenType::Keyword},

                {"std_logic", TokenType::StandardType}, {"std_logic_vector", TokenType::StandardType},
                {"integer", TokenType::StandardType}, {"real", TokenType::StandardType},
                {"boolean", TokenType::StandardType},
                {"character", TokenType::StandardType}, {"string", TokenType::StandardType},
                {"signed", TokenType::StandardType}, {"unsigned", TokenType::StandardType},

                {"rising_edge", TokenType::StandardFunction}, {"falling_edge", TokenType::StandardFunction},
                {"to_unsigned", TokenType::StandardFunction}, {"to_signed", TokenType::StandardFunction},

                {"left", TokenType::Attribute}, {"right", TokenType::Attribute},
                {"low", TokenType::Attribute}, {"high", TokenType::Attribute},
                {"length", TokenType::Attribute},

                {"and", TokenType::Operator}, {"or", TokenType::Operator},
                {"nand", TokenType::Operator}, {"nor", TokenType::Operator},
                {"xor", TokenType::Operator}, {"xnor", TokenType::Operator},
                {"not", TokenType::Operator}, {"sll", TokenType::Operator},
                {"srl", TokenType::Operator}, {"sra", TokenType::Operator},
                {"rol", TokenType::Operator}, {"ror", TokenType::Operator}
            };

            auto it = keywordMap.find(identifier);
            if (it != keywordMap.end())
            {
                return it->second;
            }
            return TokenType::Identifier;
        }
    }

    void Tokenizer::advance(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (index < source.size())
            {
                if (source[index] == '\n')
                {
                    currentLine++;
                    currentColumn = 1;
                }
                else
                {
                    currentColumn++;
                }
                index++;
            }
        }
    }

    void Tokenizer::skipWhitespace(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        while (index < source.size() && std::isspace(static_cast<unsigned char>(source[index])))
        {
            advance(source, index, currentLine, currentColumn);
        }
    }

    void Tokenizer::skipLineComment(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        if (source[index] == '-' && index + 1 < source.size() && source[index + 1] == '-')
        {
            advance(source, index, currentLine, currentColumn, 2);
            while (index < source.size() && source[index] != '\n')
            {
                advance(source, index, currentLine, currentColumn);
            }
        }
    }

    bool Tokenizer::tokenizeIdentifierOrKeyword(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        if (!isIdentifierStart(source[index]))
            return false;

        size_t startLine = currentLine;
        size_t startColumn = currentColumn;
        size_t start = index;

        advance(source, index, currentLine, currentColumn);
        while (index < source.size() && isIdentifierChar(source[index]))
        {
            advance(source, index, currentLine, currentColumn);
        }

        std::string identifier = source.substr(start, index - start);
        toLowerInPlace(identifier);

        if (index < source.size() && source[index] == '"')
        {
            if (identifier == "b" || identifier == "o" || identifier == "x" || identifier == "d" ||
                identifier == "ub" || identifier == "uo" || identifier == "ux" ||
                identifier == "sb" || identifier == "so" || identifier == "sx")
            {
                size_t strStart = index;
                advance(source, index, currentLine, currentColumn);

                while (index < source.size() && source[index] != '"')
                {
                    if (source[index] == '\n')
                        throw std::runtime_error("Tokenizer: unterminated bit string literal.");
                    advance(source, index, currentLine, currentColumn);
                }

                if (index >= source.size())
                    throw std::runtime_error("Tokenizer: unterminated bit string literal.");

                advance(source, index, currentLine, currentColumn);
                std::string bitString = identifier + source.substr(strStart, index - strStart);
                tokens.push_back({ TokenType::BitStringLiteral, std::move(bitString), startLine, startColumn });
                return true;
            }
        }

        TokenType type = classifyIdentifier(identifier);
        tokens.push_back({ type, std::move(identifier), startLine, startColumn });
        return true;
    }

    bool Tokenizer::tokenizeNumericLiteral(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        if (!std::isdigit(static_cast<unsigned char>(source[index])))
            return false;

        size_t startLine = currentLine;
        size_t startColumn = currentColumn;
        size_t start = index;

        advance(source, index, currentLine, currentColumn);
        while (index < source.size() &&
            (std::isalnum(static_cast<unsigned char>(source[index])) ||
            source[index] == '_' || source[index] == '.' || source[index] == '#' ||
            ((source[index] == '+' || source[index] == '-') &&
            (source[index - 1] == 'e' || source[index - 1] == 'E'))))
        {
            advance(source, index, currentLine, currentColumn);
        }

        std::string number = source.substr(start, index - start);
        toLowerInPlace(number);

        if (index < source.size() && source[index] == '"')
        {
            if (number.back() == 'b' || number.back() == 'o' || number.back() == 'x' || number.back() == 'd')
            {
                size_t strStart = index;
                advance(source, index, currentLine, currentColumn);

                while (index < source.size() && source[index] != '"')
                {
                    if (source[index] == '\n')
                        throw std::runtime_error("Tokenizer: unterminated sized bit string literal.");
                    advance(source, index, currentLine, currentColumn);
                }

                if (index >= source.size())
                    throw std::runtime_error("Tokenizer: unterminated sized bit string literal.");

                advance(source, index, currentLine, currentColumn);
                std::string bitString = number + source.substr(strStart, index - strStart);
                tokens.push_back({ TokenType::BitStringLiteral, std::move(bitString), startLine, startColumn });
                return true;
            }
        }

        tokens.push_back({ TokenType::NumericLiteral, std::move(number), startLine, startColumn });
        return true;
    }

    bool Tokenizer::tokenizeStringLiteral(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        if (source[index] != '"')
            return false;

        size_t startLine = currentLine;
        size_t startColumn = currentColumn;
        size_t start = index;

        advance(source, index, currentLine, currentColumn);
        while (index < source.size() && source[index] != '"')
        {
            if (source[index] == '\n')
                throw std::runtime_error("Tokenizer: unterminated string literal.");
            advance(source, index, currentLine, currentColumn);
        }

        if (index >= source.size())
            throw std::runtime_error("Tokenizer: unterminated string literal.");

        advance(source, index, currentLine, currentColumn);
        tokens.push_back({ TokenType::BitStringLiteral, source.substr(start, index - start), startLine, startColumn });
        return true;
    }

    bool Tokenizer::tokenizeCharacterOrTick(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        if (source[index] != '\'')
            return false;

        size_t startLine = currentLine;
        size_t startColumn = currentColumn;

        if (index + 2 < source.size() && source[index + 2] == '\'')
        {
            tokens.push_back({ TokenType::CharacterLiteral, source.substr(index, 3), startLine, startColumn });
            advance(source, index, currentLine, currentColumn, 3);
        }
        else
        {
            tokens.push_back({ TokenType::Operator, "'", startLine, startColumn });
            advance(source, index, currentLine, currentColumn);
        }
        return true;
    }

    bool Tokenizer::tokenizeTwoCharOperator(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        if (index + 1 >= source.size())
            return false;

        size_t startLine = currentLine;
        size_t startColumn = currentColumn;
        std::string two = source.substr(index, 2);

        if (two == ":=" || two == "<=" || two == ">=" ||
            two == "=>" || two == "/=" || two == "**")
        {
            tokens.push_back({ TokenType::Operator, two, startLine, startColumn });
            advance(source, index, currentLine, currentColumn, 2);
            return true;
        }
        return false;
    }

    bool Tokenizer::tokenizeDelimiter(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        static const std::string delimiters = "()[],;:.";
        if (delimiters.find(source[index]) != std::string::npos)
        {
            size_t startLine = currentLine;
            size_t startColumn = currentColumn;
            tokens.push_back({ TokenType::Delimiter, std::string(1, source[index]), startLine, startColumn });
            advance(source, index, currentLine, currentColumn);
            return true;
        }
        return false;
    }

    bool Tokenizer::tokenizeSingleCharOperator(const std::string& source, size_t& index, size_t& currentLine, size_t& currentColumn)
    {
        static const std::string operators = "&+-*/<>=|";
        if (operators.find(source[index]) != std::string::npos)
        {
            size_t startLine = currentLine;
            size_t startColumn = currentColumn;
            tokens.push_back({ TokenType::Operator, std::string(1, source[index]), startLine, startColumn });
            advance(source, index, currentLine, currentColumn);
            return true;
        }
        return false;
    }

    Tokenizer::Tokenizer(std::istream& input)
    {
        std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

        size_t index = 0;
        size_t currentLine = 1;
        size_t currentColumn = 1;

        while (index < source.size())
        {
            // Keep skipping whitespace and comments until we hit actual content
            bool madeProgress = true;
            while (madeProgress && index < source.size())
            {
                size_t startIndex = index;
                skipWhitespace(source, index, currentLine, currentColumn);
                if (index < source.size())
                    skipLineComment(source, index, currentLine, currentColumn);
                madeProgress = (index > startIndex);
            }

            if (index >= source.size())
                break;

            if (tokenizeIdentifierOrKeyword(source, index, currentLine, currentColumn))
                continue;
            if (tokenizeNumericLiteral(source, index, currentLine, currentColumn))
                continue;
            if (tokenizeStringLiteral(source, index, currentLine, currentColumn))
                continue;
            if (tokenizeCharacterOrTick(source, index, currentLine, currentColumn))
                continue;
            if (tokenizeTwoCharOperator(source, index, currentLine, currentColumn))
                continue;
            if (tokenizeDelimiter(source, index, currentLine, currentColumn))
                continue;
            if (tokenizeSingleCharOperator(source, index, currentLine, currentColumn))
                continue;

            throw std::runtime_error("Tokenizer: unexpected character '" + std::string(1, source[index]) +
                "' at line " + std::to_string(currentLine) +
                ", column " + std::to_string(currentColumn) + ".");
        }
    }

    Tokenizer::~Tokenizer() = default;

    size_t Tokenizer::size() const
    {
        return tokens.size();
    }

    void Tokenizer::reset()
    {
        currentIndex = 0;
    }

    bool Tokenizer::operator>>(Token& token)
    {
        if (currentIndex >= tokens.size())
            return false;

        token = tokens[currentIndex++];
        return true;
    }

} // namespace Pulse::Parser::VHDL