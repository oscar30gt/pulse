#include "parser_impl.h"
#include "bitstring.h"

#include <algorithm>

// Expansion of string literals (LRM 15.7 and 15.8). A plain string is its characters; a bit string is the
// string its prefix expands the digits to, e.g. x"F" is "1111" and 8sx"F" is "11111111".

namespace Pulse::Parser
{
    namespace
    {
        /// Longest string an explicit bit-string width may ask for, so `4000000000b"1"` cannot exhaust memory.
        constexpr size_t maxBitStringWidth = 1u << 20;

        [[noreturn]] void fail(const Token& token, const std::string& message)
        {
            throw ast_syntax_error(message, locationOf(&token));
        }

        /// `""` inside a plain string stands for one quote.
        std::string unescapeQuotes(std::string_view body)
        {
            std::string text;
            for (size_t i = 0; i < body.size(); ++i)
            {
                text += body[i];
                if (body[i] == '"')
                    ++i;
            }
            return text;
        }

        std::string withoutUnderscores(std::string_view body)
        {
            std::string digits;
            for (char c : body)
                if (c != '_')
                    digits += c;
            return digits;
        }

        /// Value of `c` as a digit of `radix`, or -1 when it is an extended digit (a logic state such as 'Z').
        int digitOf(BitStringRadix radix, char c)
        {
            const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const int value = (lower >= '0' && lower <= '9') ? lower - '0'
                            : (lower >= 'a' && lower <= 'f') ? lower - 'a' + 10
                            : -1;
            const int limit = radix == BitStringRadix::Binary ? 2 : radix == BitStringRadix::Octal ? 8 : 16;
            return value < limit ? value : -1;
        }

        /// Binary, octal and hexadecimal: every digit becomes its bits; an extended digit is repeated as written.
        std::string expandDigits(BitStringRadix radix, const std::string& digits)
        {
            const int width = bitsPerDigit(radix);
            std::string bits;
            for (char c : digits)
            {
                const int value = digitOf(radix, c);
                if (value < 0)
                {
                    bits.append(static_cast<size_t>(width), c);
                    continue;
                }
                for (int bit = width - 1; bit >= 0; --bit)
                    bits += ((value >> bit) & 1) ? '1' : '0';
            }
            return bits;
        }

        /// Decimal: the binary form of the number with no leading zeros, however many digits it has.
        std::string expandDecimal(const std::string& digits)
        {
            std::string number = digits;
            std::string bits;
            while (number.find_first_not_of('0') != std::string::npos)
            {
                std::string quotient;
                int remainder = 0;
                for (char c : number)
                {
                    const int current = remainder * 10 + (c - '0');
                    if (!quotient.empty() || current >= 2)
                        quotient += static_cast<char>('0' + current / 2);
                    remainder = current % 2;
                }
                bits += static_cast<char>('0' + remainder);
                number = quotient;
            }
            std::reverse(bits.begin(), bits.end());
            return bits.empty() && !digits.empty() ? "0" : bits;
        }

        /// The left character a signed or unsigned literal is extended with.
        char fillCharacter(BitStringSign sign, const std::string& bits)
        {
            return sign == BitStringSign::Signed && !bits.empty() ? bits.front() : '0';
        }

        /// Applies an explicit width: extends on the left, or drops left characters that carry no value.
        std::string resize(const Token& token, BitStringSign sign, std::string bits, size_t width)
        {
            if (width > maxBitStringWidth)
                fail(token, "Bit string " + token.original + " is too wide (at most " + std::to_string(maxBitStringWidth) + " characters)");

            if (bits.size() <= width)
                return std::string(width - bits.size(), fillCharacter(sign, bits)) + bits;

            const size_t dropped = bits.size() - width;
            const char expected = sign == BitStringSign::Signed ? bits[dropped] : '0';
            for (size_t i = 0; i < dropped; ++i)
                if (bits[i] != expected)
                    fail(token, "Bit string " + token.original + " does not fit in " + std::to_string(width) + " characters");
            return bits.substr(dropped);
        }

        std::string expandBitString(const Token& token, std::string_view prefixText, std::string_view body)
        {
            std::string error;
            std::optional<BitStringPrefix> prefix = parseBitStringPrefix(prefixText, error);
            if (!prefix)
                fail(token, error);

            const std::string digits = withoutUnderscores(body);
            std::string bits = prefix->radix == BitStringRadix::Decimal ? expandDecimal(digits)
                                                                        : expandDigits(prefix->radix, digits);
            if (prefix->size)
                bits = resize(token, prefix->sign, std::move(bits), *prefix->size);
            return bits;
        }
    } // anonymous namespace

    std::string stringValue(const Token& token)
    {
        const std::string& text = token.value;
        const size_t quote = text.find('"');
        const std::string_view prefix(text.data(), quote);
        const std::string_view body(text.data() + quote + 1, text.size() - quote - 2);

        if (prefix.empty())
            return unescapeQuotes(body);
        return expandBitString(token, prefix, body);
    }

} // namespace Pulse::Parser
