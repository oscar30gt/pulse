#include "source_reader.h"
#include "keywords.h"
#include "bitstring.h"

#include <algorithm>
#include <cctype>

namespace Pulse::Parser
{
    namespace
    {
        bool isLetter(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c)) != 0;
        }

        bool isWordChar(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        std::string toLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }
    } // anonymous namespace

    void SourceReader::validateIdentifier(const std::string& word, const SourceMark& start) const
    {
        if (word.back() == '_')
            fail("Identifier '" + word + "' cannot end with an underscore", start);

        if (word.find("__") != std::string::npos)
            fail("Identifier '" + word + "' cannot contain consecutive underscores", start);
    }

    bool SourceReader::tokenizeWord()
    {
        if (!isLetter(current()))
            return false;

        SourceMark start = mark();

        while (!atEnd() && isWordChar(current()))
            advance();

        std::string word = toLower(source.substr(start.index, index - start.index));
        validateIdentifier(word, start);

        // A word immediately followed by a quote is a bit-string prefix (x"FF", ub"01", ...) when it is one of the
        // LRM base specifiers; any other word (`report"text"`) is just a word followed by a string.
        if (current() == '"' && isBitStringBaseSpecifier(word))
        {
            emitBitString(word, start);
            return true;
        }

        TokenType type = classifyWord(word);
        emit(type, std::move(word), start);
        return true;
    }

} // namespace Pulse::Parser
