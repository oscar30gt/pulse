#include "source_reader.h"
#include "bitstring.h"

namespace Pulse::Parser
{
    // ---- Quoted bodies and bit strings ---------------------------------------

    /// Reads a double-quoted body starting at the opening quote. A doubled quote ("") inside the
    /// body is kept as-is (two quotes) so the parser can un-escape it where it matters.
    void SourceReader::scanQuotedBody(std::string& body, const SourceMark& start)
    {
        advance(); // opening quote

        while (!atEnd() && current() != '\n' && current() != '\r')
        {
            char c = current();

            if (c == '"')
            {
                if (peek() != '"')
                {
                    advance(); // closing quote
                    return;
                }
                body += "\"\"";
                advance(2);
                continue;
            }

            size_t length = graphicLength();
            if (length == 0)
                failHere("String literals can only contain graphic characters");

            body.append(source, index, length);
            advance(length);
        }

        fail("Unterminated string literal", start);
    }

    /// Emits a bit-string token. `prefix` is empty for a plain string ("1010"); otherwise it has
    /// already been lowercased and is validated here together with the characters of the body.
    void SourceReader::emitBitString(const std::string& prefix, const SourceMark& start)
    {
        std::string error;
        std::optional<BitStringPrefix> parsed;

        if (!prefix.empty())
        {
            parsed = parseBitStringPrefix(prefix, error);
            if (!parsed)
                fail(error, start);
        }

        std::string body;
        scanQuotedBody(body, start);

        if (parsed)
        {
            error = bitStringBodyError(parsed->radix, body);
            if (!error.empty())
                fail(error, start);
        }

        emit(TokenType::StringLiteral, prefix + "\"" + body + "\"", start);
    }

    bool SourceReader::tokenizeString()
    {
        if (current() != '"')
            return false;

        emitBitString("", mark());
        return true;
    }

    // ---- Character literals and attribute ticks ------------------------------

    bool SourceReader::previousTokenAllowsTick() const
    {
        if (out.empty())
            return false;

        const Token& prev = out.back();
        switch (prev.type)
        {
            case TokenType::Identifier: return true;
            case TokenType::Keyword:    return prev.value == "all";
            case TokenType::Delimiter:  return prev.value == ")" || prev.value == "]";
            default:                    return false;
        }
    }

    bool SourceReader::tokenizeCharacterOrTick()
    {
        if (current() != '\'')
            return false;

        SourceMark start = mark();

        // Character literal: 'X' -- exactly one graphic character between the quotes -- unless the quote directly follows
        // something that can be the prefix of an attribute (`clk'event`) or qualified expression (`t'('1')`).
        size_t length = previousTokenAllowsTick() ? 0 : graphicLength(1);
        if (length > 0 && peek(1 + length) == '\'')
        {
            advance(2 + length);
            emit(TokenType::CharacterLiteral, source.substr(start.index, 2 + length), start);
        }
        else
        {
            advance();
            emit(TokenType::Operator, "'", start);
        }
        return true;
    }

} // namespace Pulse::Parser
