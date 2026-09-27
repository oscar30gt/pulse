#ifndef PULSE_PARSER_LIB_BITSTRING_H
#define PULSE_PARSER_LIB_BITSTRING_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace Pulse::Parser
{
    /// String literal format:
    ///    [width][sign][radix]'[digits]'
    ///     |      |     |
    ///     |      |     +---- Radix: b, o, d or x
    ///     |      +---------- Sign: u or s (only for b, o and x)
    ///     +----------------- Width in bits (optional)

    /// How a sized bit-string literal is extended/validated when its length differs from the digits given.
    enum class BitStringSign { None, Unsigned, Signed };

    /// The radix item of a bit-string prefix: `b`, `o`, `d` or `x`.
    enum class BitStringRadix { Binary, Octal, Decimal, Hexadecimal };

    /// Parsed form of a bit-string prefix such as `x`, `ub`, `8sx` or `16d`.
    struct BitStringPrefix
    {
        std::optional<size_t> size;             ///< Explicit length in bits, if given.
        BitStringSign sign = BitStringSign::None;
        BitStringRadix radix = BitStringRadix::Binary;
    };

    /// Parses a lowercase prefix `[width][sign][radix]`: width in bits (optional), sign `u` or `s` (optional, only with
    /// radix b, o or x) and radix `b`, `o`, `x` or `d`. On failure returns nullopt and sets `error`.
    std::optional<BitStringPrefix> parseBitStringPrefix(std::string_view prefix, std::string& error);

    /// Whether a lowercase word is an unsized base specifier (`b o x d`, optionally preceded by `u` or `s`), i.e. can directly precede
    /// a quote to form a bit string.
    bool isBitStringBaseSpecifier(std::string_view word);

    /// Whether `c` may appear in the body of a bit string of the given radix (underscores are always allowed).
    bool isBitStringChar(BitStringRadix radix, char c);

    /// Checks a bit-string body: every character must be valid for the base, and an underscore may only sit between
    /// two other characters (`1_0`, never `_1`, `1_` or `1__0`). Returns an error message, or "" when the body is valid.
    std::string bitStringBodyError(BitStringRadix radix, std::string_view body);

    /// Number of bits each digit of the radix expands to (binary 1, octal 3, hexadecimal 4). Zero for decimal.
    int bitsPerDigit(BitStringRadix radix);

    /// Human readable radix name for messages ("binary", "octal", "hexadecimal", "decimal").
    const char* bitStringRadixName(BitStringRadix radix);

} // namespace Pulse::Parser

#endif // PULSE_PARSER_LIB_BITSTRING_H
