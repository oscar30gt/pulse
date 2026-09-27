#include "source_reader.h"

#include <istream>

namespace Pulse::Parser
{
    // -------- Tokenizer Implementation ----------------------------------------------------------

    Tokenizer::Tokenizer(std::istream& input, size_t file)
        : Tokenizer(std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()), file)
    { }

    Tokenizer::Tokenizer(const std::string& input, size_t file)
    {
        SourceReader reader(input, m_tokens, file);
        SourceMark end = reader.tokenize();
        
        m_endOfFile = { TokenType::EndOfFile, "", "", end.line, end.column, file };
    }

    Tokenizer::~Tokenizer() = default;

    size_t Tokenizer::index() const
    {
        return m_currentIndex;
    }

    void Tokenizer::seek(size_t position)
    {
        m_currentIndex = position < m_tokens.size() ? position : m_tokens.size();
    }

    size_t Tokenizer::size() const
    {
        return m_tokens.size();
    }

    size_t Tokenizer::remaining() const
    {
        return m_tokens.size() - m_currentIndex;
    }

    const Token* Tokenizer::eof() const
    {
        return &m_endOfFile;
    }

    const Token* Tokenizer::next(size_t count)
    {
        if (count > m_tokens.size() - m_currentIndex)
        {
            m_currentIndex = m_tokens.size();
            return &m_endOfFile;
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
            return &m_endOfFile;

        return &m_tokens[peekPos];
    }

    bool Tokenizer::end() const
    {
        return m_currentIndex >= m_tokens.size();
    }

} // namespace Pulse::Parser