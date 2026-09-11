#include "tokenizer_internal.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace Pulse::Parser
{
    namespace
    {
        /// Lowers a string in place.
        /// @param s Reference to the string to lower.
        void toLowerInPlace(std::string& s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return std::tolower(c); }
            );
        }

        /// Validate that @p prefix is a well-formed sized bit-string prefix.
        ///
        /// Grammar (after lowercasing):
        ///   sized-prefix ::= <digits> [u|s] <base-letter>
        ///   base-letter  ::= b | o | x | d
        ///
        /// Examples of valid prefixes : "8x", "12ux", "12sx", "4b", "8sb"
        /// Examples of invalid prefixes: "abc", "12z", "12abx", ""
        ///
        /// @param prefix Lowercased string ending with the base letter (already
        ///               confirmed to end with b/o/x/d by the caller).
        /// @returns True when the prefix is a valid sized bit-string prefix.
        bool isSizedBitStringPrefix(const std::string& prefix)
        {
            if (prefix.empty())
                return false;

            // The last character is the base letter (b/o/x/d) — already
            // confirmed by callers, but checked here for robustness.
            char base = prefix.back();
            if (base != 'b' && base != 'o' && base != 'x' && base != 'd')
                return false;

            // Everything before the base letter must be: <digits> [u|s]
            std::string_view body(prefix.data(), prefix.size() - 1);

            if (body.empty())
                return false;

            // Optional signedness qualifier (last char of body)
            if (body.back() == 'u' || body.back() == 's')
                body.remove_suffix(1);

            if (body.empty())
                return false;

            // Remaining characters must all be decimal digits.
            for (char c : body)
                if (!std::isdigit(static_cast<unsigned char>(c)))
                    return false;

            return true;
        }

        /// Scan and emit a quoted body starting at the current position (which
        /// must already be pointing at the opening '"').  The full token value
        /// is @p leader + the quoted body (including both quote characters).
        ///
        /// @param reader   The owning SourceReader (provides advance/atEnd/current).
        /// @param leader   Text already consumed before the opening quote.
        /// @param startLine   Source line where the token started.
        /// @param startCol    Source column where the token started.
        /// @param errorTag    Tag used in error messages ("bit string literal", etc.).
        ///
        /// This helper exists solely to avoid duplicating the
        /// unterminated-literal error handling across the three call sites.
        ///
        /// Because SourceReader's navigation methods are private, this helper
        /// is a free function that takes explicit references — the callers use
        /// it via a small lambda wrapper defined at each call site.
        ///
        /// @note We avoid giving this a SourceReader* parameter to keep it
        /// independent of the class layout; callers pass lambdas instead.

    } // anonymous namespace

    // ---- Numeric literals ---------------------------------------------------

    bool SourceReader::tokenizeNumericLiteral()
    {
        if (!std::isdigit(static_cast<unsigned char>(current())))
            return false;

        size_t startLine   = line;
        size_t startColumn = column;
        size_t start       = index;

        advance();
        while (!atEnd() && (
            std::isalnum(static_cast<unsigned char>(current())) ||      // alnum (hex, octal, …)
            current() == '_' || current() == '.' || current() == '#' || // separators / decimal
            ((current() == '+' || current() == '-') &&                  // sign after exponent
             (source[index - 1] == 'e' || source[index - 1] == 'E'))
        ))
        {
            advance();
        }

        std::string number = source.substr(start, index - start);
        toLowerInPlace(number);

        // Sized bit-string literal: e.g. 8x"FF", 12sx"0A", 4ub"1010"
        // The numeric scan above consumes the size digits and the optional
        // signedness/base suffix (e.g. "12sx"), so we just need to check
        // that the result is a valid sized prefix followed by a quoted body.
        if (!atEnd() && current() == '"' && isSizedBitStringPrefix(number))
        {
            size_t strStart = index;
            advance(); // consume opening '"'

            while (!atEnd() && current() != '"')
            {
                if (current() == '\n')
                    throw std::runtime_error(
                        "Tokenizer: unterminated sized bit-string literal at line "
                        + std::to_string(line) + ", column " + std::to_string(column) + ".");
                advance();
            }

            if (atEnd())
                throw std::runtime_error(
                    "Tokenizer: unterminated sized bit-string literal at line "
                    + std::to_string(line) + ", column " + std::to_string(column) + ".");

            advance(); // consume closing '"'
            emit(TokenType::BitStringLiteral,
                 number + source.substr(strStart, index - strStart),
                 startLine, startColumn);
            return true;
        }

        emit(TokenType::NumericLiteral, std::move(number), startLine, startColumn);
        return true;
    }

    // ---- Unquoted (plain) string literals -----------------------------------

    bool SourceReader::tokenizeBitStringLiteral()
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
        emit(TokenType::BitStringLiteral, source.substr(start, index - start), startLine, startColumn);
        return true;
    }

    // ---- Character literals and attribute ticks -----------------------------

    bool SourceReader::tokenizeCharacterOrTick()
    {
        if (current() != '\'')
            return false;

        size_t startLine   = line;
        size_t startColumn = column;

        // Character literal: 'X' — exactly one character between the quotes.
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

} // namespace Pulse::Parser
