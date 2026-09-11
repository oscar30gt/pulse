#include "tokenizer_internal.h"

#include <cctype>

namespace Pulse::Parser
{
    SourceReader::SourceReader(const std::string& source, std::vector<Token>& out)
        : source(source), out(out)
    { }

    // ---- Low-level navigation -----------------------------------------------

    char SourceReader::current() const
    {
        return source[index];
    }

    char SourceReader::peek(size_t offset) const
    {
        return source[index + offset];
    }

    bool SourceReader::atEnd() const
    {
        return index >= source.size();
    }

    bool SourceReader::atEnd(size_t offset) const
    {
        return index + offset >= source.size();
    }

    void SourceReader::advance(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (index >= source.size())
                continue;

            if (source[index] == '\n')
            {
                line++;
                column = 1;
            }
            else
            {
                column++;
            }
            index++;
        }
    }

    // ---- Token emission -----------------------------------------------------

    void SourceReader::emit(TokenType type, std::string value, size_t startLine, size_t startColumn)
    {
        out.push_back({ type, std::move(value), startLine, startColumn });
    }

    // ---- Skip helpers -------------------------------------------------------

    void SourceReader::skipWhitespace()
    {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(current())))
            advance();
    }

    bool SourceReader::skipLineComment()
    {
        if (atEnd(1) || current() != '-' || peek() != '-')
            return false;

        advance(2);
        while (!atEnd() && current() != '\n')
            advance();

        return true;
    }

} // namespace Pulse::Parser
