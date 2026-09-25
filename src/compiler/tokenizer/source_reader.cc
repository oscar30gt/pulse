#include "source_reader.h"

#include <cctype>

namespace Pulse::Parser
{
    SourceReader::SourceReader(const std::string& source, std::vector<Token>& out)
        : source(source), out(out)
    { }

    // ---- Low-level navigation -----------------------------------------------

    char SourceReader::current() const
    {
        return index < source.size() ? source[index] : '\0';
    }

    char SourceReader::peek(size_t offset) const
    {
        return index + offset < source.size() ? source[index + offset] : '\0';
    }

    bool SourceReader::atEnd() const
    {
        return index >= source.size();
    }

    void SourceReader::advance(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (index >= source.size())
                return;

            char c = source[index];
            if (c == '\n')
            {
                line++;
                column = 1;
            }
            else if (c == '\r')
            {
                // "\r\n" is a single line break, counted when the '\n' is consumed; a lone '\r' is one by itself.
                if (index + 1 >= source.size() || source[index + 1] != '\n')
                {
                    line++;
                    column = 1;
                }
            }
            else
            {
                column++;
            }
            index++;
        }
    }

    size_t SourceReader::graphicLength(size_t offset) const
    {
        if (index + offset >= source.size())
            return 0;

        unsigned char lead = static_cast<unsigned char>(source[index + offset]);
        if (lead < 0x20 || lead == 0x7F)
            return 0;
        if (lead < 0x80)
            return 1;

        size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        for (size_t i = 1; i < length; ++i)
        {
            size_t at = index + offset + i;
            if (at >= source.size() || (static_cast<unsigned char>(source[at]) & 0xC0) != 0x80)
                return 1; // not a well-formed sequence: a single Latin-1 style byte
        }
        return length;
    }

    // ---- Token emission & errors --------------------------------------------

    SourceMark SourceReader::mark() const
    {
        return { index, line, column };
    }

    void SourceReader::emit(TokenType type, std::string value, const SourceMark& start)
    {
        out.push_back({
            type,
            std::move(value),
            source.substr(start.index, index - start.index),
            start.line, start.column
        });
    }

    void SourceReader::fail(const std::string& message, size_t atLine, size_t atColumn) const
    {
        throw ast_lexical_error(message, { atLine, atColumn });
    }

    void SourceReader::fail(const std::string& message, const SourceMark& at) const
    {
        fail(message, at.line, at.column);
    }

    void SourceReader::failHere(const std::string& message) const
    {
        fail(message, line, column);
    }

    // ---- Trivia --------------------------------------------------------------

    void SourceReader::skipWhitespace()
    {
        while (!atEnd())
        {
            unsigned char c = static_cast<unsigned char>(current());
            if (std::isspace(c))
                advance();
            else if (c == 0xC2 && static_cast<unsigned char>(peek()) == 0xA0)
                advance(2); // UTF-8 non-breaking space
            else
                break;
        }
    }

    bool SourceReader::skipLineComment()
    {
        if (current() != '-' || peek() != '-')
            return false;

        advance(2);
        while (!atEnd() && current() != '\n' && current() != '\r')
            advance();

        return true;
    }

    bool SourceReader::skipBlockComment()
    {
        if (current() != '/' || peek() != '*')
            return false;

        SourceMark start = mark();
        advance(2);

        while (!atEnd())
        {
            if (current() == '*' && peek() == '/')
            {
                advance(2);
                return true;
            }
            advance();
        }

        fail("Unterminated block comment", start);
    }

    void SourceReader::skipTrivia()
    {
        size_t before;
        do
        {
            before = index;
            skipWhitespace();
            skipLineComment();
            skipBlockComment();
        } while (index != before);
    }

    void SourceReader::failUnexpectedCharacter() const
    {
        if (current() == '_')
            failHere("Identifiers cannot start with an underscore");

        if (current() == '\\')
            failHere("Extended identifiers (\\name\\) are not supported");

        unsigned char c = static_cast<unsigned char>(current());
        if (c < 0x20 || c == 0x7F)
            failHere("Unexpected control character (code " + std::to_string(c) + ")");

        failHere(std::string("Unexpected character '") + current() + "'");
    }

    // ---- Main tokenization loop ---------------------------------------------

    SourceMark SourceReader::tokenize()
    {
        while (true)
        {
            skipTrivia();
            if (atEnd())
                break;

            bool matched = tokenizeWord()
                        || tokenizeNumber()
                        || tokenizeString()
                        || tokenizeCharacterOrTick()
                        || tokenizeCompoundOperator()
                        || tokenizeDelimiter()
                        || tokenizeSingleCharOperator();

            if (!matched)
                failUnexpectedCharacter();
        }
        return mark();
    }

} // namespace Pulse::Parser
