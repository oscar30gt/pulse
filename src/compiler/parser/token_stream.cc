#include "token_stream.h"

namespace Pulse::Parser
{
    namespace
    {
        /// Reserved words, operators and delimiters: the tokens `at` can match by value.
        bool isFixedToken(TokenType type)
        {
            return type == TokenType::Keyword || type == TokenType::Operator || type == TokenType::Delimiter;
        }
    } // anonymous namespace

    TokenStream::TokenStream(Tokenizer& tokenizer)
        : m_tokenizer(tokenizer)
    { }

    // ---- Looking --------------------------------------------------------------------------------

    const Token* TokenStream::peek(size_t offset) const
    {
        const Token* token = m_tokenizer.peek(static_cast<ssize_t>(offset));
        return token ? token : m_tokenizer.eof();
    }

    size_t TokenStream::position() const
    {
        return m_tokenizer.index();
    }

    bool TokenStream::atEnd() const
    {
        return peek()->type == TokenType::EndOfFile;
    }

    bool TokenStream::at(std::string_view word, size_t offset) const
    {
        const Token* token = peek(offset);
        return isFixedToken(token->type) && token->value == word;
    }

    bool TokenStream::atAny(std::initializer_list<std::string_view> words) const
    {
        for (std::string_view word : words)
            if (at(word))
                return true;
        return false;
    }

    bool TokenStream::atType(TokenType type, size_t offset) const
    {
        return peek(offset)->type == type;
    }

    // ---- Consuming ------------------------------------------------------------------------------

    const Token* TokenStream::next()
    {
        const Token* token = peek();
        if (!atEnd())
            m_tokenizer.next();
        return token;
    }

    const Token* TokenStream::accept(std::string_view word)
    {
        return at(word) ? next() : nullptr;
    }

    const Token* TokenStream::expect(std::string_view word)
    {
        if (!at(word))
            expected("'" + std::string(word) + "'");
        return next();
    }

    const Token* TokenStream::expectIdentifier()
    {
        if (!atType(TokenType::Identifier))
            expected("an identifier");
        return next();
    }

    void TokenStream::seek(size_t position)
    {
        m_tokenizer.seek(position);
    }

    // ---- Errors ---------------------------------------------------------------------------------

    void TokenStream::expected(std::string_view what) const
    {
        fail("Expected " + std::string(what) + ", but found " + describe(peek()));
    }

    void TokenStream::fail(const std::string& message) const
    {
        failAt(peek(), message);
    }

    void TokenStream::failAt(const Token* token, const std::string& message) const
    {
        throw ast_syntax_error(message, locationOf(token));
    }

    std::string describe(const Token* token)
    {
        if (token->type == TokenType::EndOfFile)
            return "the end of the file";
        return "'" + token->original + "'";
    }

    SourceLocation locationOf(const Token* token)
    {
        return { token->line, token->column };
    }

} // namespace Pulse::Parser
