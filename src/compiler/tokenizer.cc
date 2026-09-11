#include "tokenizer_internal.h"

#include <istream>

namespace Pulse::Parser
{
    // -------- Tokenizer Implementation ----------------------------------------------------------

    Tokenizer::Tokenizer(std::istream& input)
        : Tokenizer(std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()))
    { }

    Tokenizer::Tokenizer(const std::string& input)
    {
        SourceReader reader(input, m_tokens);
        reader.tokenize();
    }

    Tokenizer::~Tokenizer() = default;

    size_t Tokenizer::index() const
    {
        return m_currentIndex;
    }

    size_t Tokenizer::size() const
    {
        return m_tokens.size();
    }

    size_t Tokenizer::remaining() const
    {
        return m_tokens.size() - m_currentIndex;
    }

    const Token* Tokenizer::next(size_t count)
    {
        if (count > m_tokens.size() - m_currentIndex)
        {
            m_currentIndex = m_tokens.size();
            return nullptr;
        }
        m_currentIndex += count;
        return &m_tokens[m_currentIndex - 1];
    }

    const Token* Tokenizer::prev(size_t count)
    {
        if (count > m_currentIndex)
        {
            m_currentIndex = 0;
            return nullptr;
        }
        m_currentIndex -= count;
        return &m_tokens[m_currentIndex];
    }

    const Token* Tokenizer::peek(ssize_t offset) const
    {
        if (offset < 0)
        {
            size_t back = static_cast<size_t>(-offset);
            if (back > m_currentIndex)
                return nullptr;
            return &m_tokens[m_currentIndex - back];
        }

        size_t peekPos = m_currentIndex + static_cast<size_t>(offset);
        if (peekPos >= m_tokens.size())
            return nullptr;

        return &m_tokens[peekPos];
    }

    bool Tokenizer::end() const
    {
        return m_currentIndex >= m_tokens.size();
    }

} // namespace Pulse::Parser