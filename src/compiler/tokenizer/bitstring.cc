#include "bitstring.h"

#include <cctype>
#include <cstdint>

namespace Pulse::Parser
{
    namespace
    {
        bool isRadixLetter(char c)
        {
            return c == 'b' || c == 'o' || c == 'x' || c == 'd';
        }

        BitStringRadix radixOf(char letter)
        {
            switch (letter)
            {
                case 'o': return BitStringRadix::Octal;
                case 'd': return BitStringRadix::Decimal;
                case 'x': return BitStringRadix::Hexadecimal;
                default:  return BitStringRadix::Binary;
            }
        }

        /// Characters other than digits that a std_logic bit string may contain.
        bool isLogicStateChar(char upper)
        {
            switch (upper)
            {
                case 'U': case 'X': case 'Z': case 'W': case 'L': case 'H': case '-': return true;
                default: return false;
            }
        }

        std::optional<size_t> parseSize(std::string_view digits, std::string& error)
        {
            size_t value = 0;
            for (char c : digits)
            {
                if (value > (SIZE_MAX - 9) / 10)
                {
                    error = "Bit string size '" + std::string(digits) + "' is too large";
                    return std::nullopt;
                }
                value = value * 10 + static_cast<size_t>(c - '0');
            }
            if (value == 0)
            {
                error = "Bit string size must be greater than zero";
                return std::nullopt;
            }
            return value;
        }
    } // anonymous namespace

    std::optional<BitStringPrefix> parseBitStringPrefix(std::string_view prefix, std::string& error)
    {
        BitStringPrefix result;
        size_t idx = 0;

        while (idx < prefix.size() && std::isdigit(static_cast<unsigned char>(prefix[idx])))
            ++idx;

        if (idx > 0)
        {
            result.size = parseSize(prefix.substr(0, idx), error);
            if (!result.size) return std::nullopt;
        }

        if (idx < prefix.size() && (prefix[idx] == 'u' || prefix[idx] == 's'))
        {
            result.sign = prefix[idx] == 'u' ? BitStringSign::Unsigned : BitStringSign::Signed;
            ++idx;
        }

        if (idx + 1 != prefix.size() || !isRadixLetter(prefix[idx]))
        {
            error = "Invalid bit-string prefix '" + std::string(prefix) +
                    "' (expected [size][u|s] followed by b, o, x or d)";
            return std::nullopt;
        }

        result.radix = radixOf(prefix[idx]);

        // The decimal radix has no sign: `[width][u|s]` only applies to b, o and x.
        if (result.radix == BitStringRadix::Decimal && result.sign != BitStringSign::None)
        {
            error = "Invalid bit-string prefix '" + std::string(prefix) +
                    "' (the decimal radix 'd' cannot be combined with 'u' or 's')";
            return std::nullopt;
        }

        return result;
    }

    bool isBitStringBaseSpecifier(std::string_view word)
    {
        if (word.size() == 1)
            return isRadixLetter(word[0]);
        return word.size() == 2 && (word[0] == 'u' || word[0] == 's') && isRadixLetter(word[1]);
    }

    std::string bitStringBodyError(BitStringRadix radix, std::string_view body)
    {
        for (size_t i = 0; i < body.size(); ++i)
        {
            char c = body[i];
            if (!isBitStringChar(radix, c))
            {
                return std::string("Invalid character '") + c + "' in " + bitStringRadixName(radix) +
                       " bit string literal";
            }
            if (c == '_' && (i == 0 || i + 1 == body.size() || body[i - 1] == '_'))
                return "Misplaced underscore in bit string literal (only single underscores between characters are allowed)";
        }
        return "";
    }

    bool isBitStringChar(BitStringRadix radix, char c)
    {
        if (c == '_') return true;

        char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        bool isDigit = std::isdigit(static_cast<unsigned char>(upper)) != 0;

        switch (radix)
        {
            case BitStringRadix::Decimal:     return isDigit;
            case BitStringRadix::Binary:      return upper == '0' || upper == '1' || isLogicStateChar(upper);
            case BitStringRadix::Octal:       return (isDigit && upper <= '7') || isLogicStateChar(upper);
            case BitStringRadix::Hexadecimal: return isDigit || (upper >= 'A' && upper <= 'F') || isLogicStateChar(upper);
        }
        return false;
    }

    int bitsPerDigit(BitStringRadix radix)
    {
        switch (radix)
        {
            case BitStringRadix::Binary:      return 1;
            case BitStringRadix::Octal:       return 3;
            case BitStringRadix::Hexadecimal: return 4;
            case BitStringRadix::Decimal:     return 0;
        }
        return 0;
    }

    const char* bitStringRadixName(BitStringRadix radix)
    {
        switch (radix)
        {
            case BitStringRadix::Binary:      return "binary";
            case BitStringRadix::Octal:       return "octal";
            case BitStringRadix::Hexadecimal: return "hexadecimal";
            case BitStringRadix::Decimal:     return "decimal";
        }
        return "";
    }

} // namespace Pulse::Parser
