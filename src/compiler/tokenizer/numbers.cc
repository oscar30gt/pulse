#include "source_reader.h"

#include <cctype>

namespace Pulse::Parser
{
    namespace
    {
        bool isDecimalDigit(char c)
        {
            return c >= '0' && c <= '9';
        }

        bool isWordChar(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        /// Value of a digit character (0-35), or -1 if it is not alphanumeric.
        int digitValue(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'z') return c - 'a' + 10;
            if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
            return -1;
        }

        char toLowerChar(char c)
        {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    } // anonymous namespace

    // ---- Building blocks -----------------------------------------------------

    /// Reads `digit { [_] digit }` in the given base and returns it lowercased without underscores.
    std::string SourceReader::scanDigits(int base)
    {
        SourceMark start = mark();
        std::string digits;

        while (true)
        {
            int value = digitValue(current());
            if (value >= 0 && value < base)
            {
                digits += toLowerChar(current());
                advance();
            }
            else if (value >= base && base != 10)
            {
                fail(std::string("Digit '") + current() + "' is not valid in base " + std::to_string(base),
                     line, column);
            }
            else if (current() == '_' && !digits.empty() && digitValue(peek()) >= 0 && digitValue(peek()) < base)
            {
                advance();
            }
            else
            {
                break;
            }
        }

        if (digits.empty())
            fail("Expected a digit in numeric literal", start);

        if (current() == '_')
            failHere("Misplaced underscore in numeric literal '" + digits + "_'");

        return digits;
    }

    /// Reads an optional exponent (`e`, optional sign, digits) and returns it normalized ("" if absent).
    std::string SourceReader::scanExponent()
    {
        if (toLowerChar(current()) != 'e')
            return "";

        std::string text = "e";
        advance();

        if (current() == '+' || current() == '-')
        {
            text += current();
            advance();
        }

        if (!isDecimalDigit(current()))
            failHere("Malformed exponent in numeric literal (digits expected after 'e')");

        return text + scanDigits(10);
    }

    /// Rejects a literal directly followed by more identifier characters (e.g. `10ns`, `12abc`).
    void SourceReader::finishNumber(const std::string& text, const SourceMark& start)
    {
        if (isWordChar(current()) || current() == '.')
        {
            std::string trailing;
            for (size_t i = 0; isWordChar(peek(i)) || peek(i) == '.'; ++i)
                trailing += peek(i);

            fail("Malformed numeric literal '" + text + trailing +
                 "' (a space is required between a number and a following identifier)", start);
        }

        emit(TokenType::NumericLiteral, text, start);
    }

    // ---- Literal forms -------------------------------------------------------

    void SourceReader::tokenizeDecimalLiteral(std::string text, const SourceMark& start)
    {
        bool isReal = false;

        if (current() == '.')
        {
            if (!isDecimalDigit(peek()))
                failHere("Real literal '" + text + ".' needs digits after the decimal point");

            advance();
            text += "." + scanDigits(10);
            isReal = true;
        }

        std::string exponent = scanExponent();
        if (!isReal && exponent.size() > 1 && exponent[1] == '-')
            fail("Integer literal '" + text + exponent + "' cannot have a negative exponent", start);

        finishNumber(text + exponent, start);
    }

    void SourceReader::tokenizeBasedLiteral(const std::string& baseDigits, const SourceMark& start)
    {
        if (baseDigits.size() > 2 || std::stoi(baseDigits) < 2 || std::stoi(baseDigits) > 16)
            fail("Base '" + baseDigits + "' of a based literal must be between 2 and 16", start);

        int base = std::stoi(baseDigits);
        advance(); // '#'

        std::string text = baseDigits + "#" + scanDigits(base);
        bool isReal = false;
        if (current() == '.')
        {
            advance();
            text += "." + scanDigits(base);
            isReal = true;
        }

        if (current() != '#')
            failHere("Unterminated based literal '" + text + "' (closing '#' expected)");
        advance();

        std::string exponent = scanExponent();
        if (!isReal && exponent.size() > 1 && exponent[1] == '-')
            fail("Integer literal '" + text + "#" + exponent + "' cannot have a negative exponent", start);

        finishNumber(text + "#" + exponent, start);
    }

    // ---- Sized bit strings ---------------------------------------------------

    /// True if the letters after the digits just read end at a quote, as in `8x"FF"` or `12sb"1"`.
    bool SourceReader::startsSizedBitString() const
    {
        size_t offset = 0;
        while (std::isalpha(static_cast<unsigned char>(peek(offset))))
            ++offset;

        return peek(offset) == '"';
    }

    void SourceReader::tokenizeSizedBitString(const std::string& sizeDigits, const SourceMark& start)
    {
        std::string prefix = sizeDigits;
        while (std::isalpha(static_cast<unsigned char>(current())))
        {
            prefix += toLowerChar(current());
            advance();
        }

        emitBitString(prefix, start);
    }

    // ---- Entry point ---------------------------------------------------------

    bool SourceReader::tokenizeNumber()
    {
        if (!isDecimalDigit(current()))
            return false;

        SourceMark start = mark();
        std::string digits = scanDigits(10);

        if (startsSizedBitString())
        {
            tokenizeSizedBitString(digits, start);
        }
        else if (current() == '#')
        {
            tokenizeBasedLiteral(digits, start);
        }
        else
        {
            tokenizeDecimalLiteral(digits, start);
        }

        return true;
    }

} // namespace Pulse::Parser
