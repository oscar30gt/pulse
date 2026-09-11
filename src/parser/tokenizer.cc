#include "tokenizer.h"
#include "sourceReader.h"

#include <istream>

namespace Pulse::Parser
{
    // -------- Tokenizer Implementation ----------------------------------------------------------

    Tokenizer::Tokenizer(std::istream& input)
        : Tokenizer(std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()))
    { }

    Tokenizer::Tokenizer(const std::string& input)
    {
        SourceReader reader(input, tokens);
        reader.tokenize();
    }

    Tokenizer::~Tokenizer() = default;

    size_t Tokenizer::index() const
    {
        return currentIndex;
    }

    size_t Tokenizer::size() const
    {
        return tokens.size();
    }

    size_t Tokenizer::remaining() const
    {
        return tokens.size() - currentIndex;
    }

    const Token* Tokenizer::next(size_t count)
    {
        if (count > tokens.size() - currentIndex)
        {
            currentIndex = tokens.size();
            return nullptr;
        }
        currentIndex += count;
        return &tokens[currentIndex - 1];
    }

    const Token* Tokenizer::prev(size_t count)
    {
        if (count > currentIndex)
        {
            currentIndex = 0;
            return nullptr;
        }
        currentIndex -= count;
        return &tokens[currentIndex];
    }

    const Token* Tokenizer::peek(ssize_t offset) const
    {
        if (offset < 0)
        {
            size_t back = static_cast<size_t>(-offset);
            if (back > currentIndex)
                return nullptr;
            return &tokens[currentIndex - back];
        }

        size_t peekPos = currentIndex + static_cast<size_t>(offset);
        if (peekPos >= tokens.size())
            return nullptr;

        return &tokens[peekPos];
    }

    bool Tokenizer::end() const
    {
        return currentIndex >= tokens.size();
    }

} // namespace Pulse::Parser