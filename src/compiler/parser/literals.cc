#include "parser_impl.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace Pulse::Parser
{
    namespace
    {
        [[noreturn]] void fail(const Token& token, const std::string& message)
        {
            throw ast_syntax_error(message, locationOf(&token));
        }

        /// Value of a (lowercase) digit, 0-15.
        int digitValue(char c)
        {
            return c <= '9' ? c - '0' : c - 'a' + 10;
        }

        /// A numeric token split into base, digits and exponent: "16#ff#e2" is { 16, "ff", 2 }.
        struct NumberParts
        {
            int base = 10;
            std::string_view integerDigits;
            std::string_view fractionDigits;
            int64_t exponent = 0;
        };

        /// Exponent text such as "e+12" or "e-3" (empty for none). Saturates far beyond any meaningful value.
        int64_t exponentValue(std::string_view text)
        {
            if (text.empty())
                return 0;

            size_t index = 1; // 'e'
            const bool negative = text[index] == '-';
            if (text[index] == '+' || text[index] == '-')
                ++index;

            int64_t value = 0;
            for (; index < text.size(); ++index)
                value = std::min<int64_t>(value * 10 + (text[index] - '0'), 1'000'000);
            return negative ? -value : value;
        }

        /// The tokenizer already validated the literal, so only its shape is read here.
        NumberParts splitNumber(std::string_view text)
        {
            NumberParts parts;
            std::string_view mantissa = text;
            std::string_view exponent;

            const size_t hash = text.find('#');
            if (hash != std::string_view::npos)
            {
                const size_t close = text.find('#', hash + 1);
                parts.base = std::atoi(std::string(text.substr(0, hash)).c_str());
                mantissa = text.substr(hash + 1, close - hash - 1);
                exponent = text.substr(close + 1);
            }
            else
            {
                const size_t e = text.find('e');
                mantissa = text.substr(0, e);
                exponent = e == std::string_view::npos ? std::string_view() : text.substr(e);
            }

            const size_t point = mantissa.find('.');
            parts.integerDigits = mantissa.substr(0, point);
            if (point != std::string_view::npos)
                parts.fractionDigits = mantissa.substr(point + 1);
            parts.exponent = exponentValue(exponent);
            return parts;
        }
    } // anonymous namespace

    // ---- Literal values -------------------------------------------------------------------------

    bool isRealLiteral(const Token& token)
    {
        return token.value.find('.') != std::string::npos;
    }

    int64_t integerValue(const Token& token)
    {
        const NumberParts parts = splitNumber(token.value);
        const uint64_t limit = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        const uint64_t base = static_cast<uint64_t>(parts.base);

        uint64_t value = 0;
        for (char c : parts.integerDigits)
        {
            const uint64_t digit = static_cast<uint64_t>(digitValue(c));
            if (value > (limit - digit) / base)
                fail(token, "Integer literal '" + token.original + "' is too large (maximum 2^63 - 1)");
            value = value * base + digit;
        }

        for (int64_t i = 0; i < parts.exponent && value != 0; ++i)
        {
            if (value > limit / base)
                fail(token, "Integer literal '" + token.original + "' is too large (maximum 2^63 - 1)");
            value *= base;
        }
        return static_cast<int64_t>(value);
    }

    double realValue(const Token& token)
    {
        const NumberParts parts = splitNumber(token.value);
        const double base = parts.base;

        double value = 0.0;
        for (char c : parts.integerDigits)
            value = value * base + digitValue(c);

        double scale = 1.0;
        for (char c : parts.fractionDigits)
        {
            scale /= base;
            value += digitValue(c) * scale;
        }

        if (parts.base == 10)
            value = std::strtod(token.value.c_str(), nullptr); // exact decimal rounding
        else
            value *= std::pow(base, static_cast<double>(parts.exponent));

        if (!std::isfinite(value))
            fail(token, "Real literal '" + token.original + "' is too large");
        return value;
    }

    std::string operatorSymbolName(const Token& token)
    {
        std::string text = stringValue(token);
        for (char& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return "\"" + text + "\"";
    }

    // ---- Literal nodes --------------------------------------------------------------------------

    /// An abstract literal, followed by a unit name for a physical literal (`10 ns`).
    ExpressionPtr Parser::parseNumericLiteral()
    {
        const Token* start = tokens.peek();
        ExpressionPtr magnitude = parseAbstractLiteral();
        if (!tokens.atType(TokenType::Identifier))
            return magnitude;

        auto physical = node<PhysicalLiteralExpr>(start);
        physical->magnitude = std::move(magnitude);
        physical->unit = tokens.next()->value;
        return physical;
    }

    ExpressionPtr Parser::parseAbstractLiteral()
    {
        if (!tokens.atType(TokenType::NumericLiteral))
            tokens.expected("a number");

        const Token* token = tokens.next();
        if (isRealLiteral(*token))
        {
            auto real = node<DoubleLiteralExpr>(token);
            real->value = realValue(*token);
            return real;
        }

        auto integer = node<IntegerLiteralExpr>(token);
        integer->value = integerValue(*token);
        return integer;
    }

    ExpressionPtr Parser::parseCharacterLiteral()
    {
        const Token* token = tokens.next();
        auto character = node<CharacterLiteralExpr>(token);
        character->value = token->value;
        return character;
    }

    ExpressionPtr Parser::parseStringLiteral()
    {
        const Token* token = tokens.next();
        auto string = node<StringLiteralExpr>(token);
        string->value = stringValue(*token);
        return string;
    }

} // namespace Pulse::Parser
