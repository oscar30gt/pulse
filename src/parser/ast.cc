#include "ast_internal.h"
#include <stdexcept>

namespace Pulse::Parser
{
    ParseContext::ParseContext(Tokenizer& tok) : m_tokenizer(tok) { }

    const Token* ParseContext::peek(ssize_t offset)
    {
        return m_tokenizer.peek(offset);
    }

    const Token* ParseContext::next()
    {
        return m_tokenizer.next();
    }

    const Token* ParseContext::expect(std::string_view v)
    {
        const Token* token = m_tokenizer.peek();
        if (!token || token->value != v)
        {
            error("Expected token '" + std::string(v) + "', but found '" + (token ? token->value : "end of input") + "'");
        }
        m_tokenizer.next();
        return token;
    }

    const Token* ParseContext::expect(std::initializer_list<std::string_view> values)
    {
        const Token* token = m_tokenizer.peek();
        if (token)
        {
            for (auto v : values)
            {
                if (token->value == v)
                {
                    m_tokenizer.next();
                    return token;
                }
            }
        }

        std::string expectedTokens;
        for (auto v : values)
        {
            if (!expectedTokens.empty())
                expectedTokens += ", ";
            expectedTokens += "'" + std::string(v) + "'";
        }

        error("Expected one of the following tokens: " + expectedTokens + ", but found '" + (token ? token->value : "end of input") + "'");
    }

    const Token* ParseContext::maybe(std::string_view v)
    {
        const Token* token = m_tokenizer.peek();
        if (token && token->value == v)
        {
            m_tokenizer.next();
            return token;
        }
        return nullptr; // Token did not match, return nullptr without advancing the cursor
    }

    const Token* ParseContext::expectIdentifier()
    {
        const Token* token = m_tokenizer.peek();
        if (!token || token->type != TokenType::Identifier)
        {
            error("Expected an identifier, but found '" + (token ? token->value : "end of input") + "'");
        }
        m_tokenizer.next();
        return token;
    }

    const Token* ParseContext::skipUntil(std::string_view v)
    {
        const Token* token = nullptr;
        while ((token = m_tokenizer.next()))
        {
            if (token->value == v)
            {
                return token;
            }
        }
        return nullptr; // Reached end of tokens without finding the value
    }

    const Token* ParseContext::skipUntil(std::string_view v, TokenType t)
    {
        const Token* token = nullptr;
        while ((token = m_tokenizer.next()))
        {
            if (token->value == v && token->type == t)
            {
                return token;
            }
        }
        return nullptr; // Reached end of tokens without finding the value and type
    }

    [[noreturn]] void ParseContext::error(std::string_view msg)
    {
        const Token* token = m_tokenizer.peek();
        SourceLocation location;
        location.line = token ? token->line : 0;
        location.column = token ? token->column : 0;
        throw ast_build_error(std::string(msg), location, "");
    }

    // --------------------------------------------------------------------------------------------

    ASTRoot ParseContext::parse()
    {
        ASTRoot root;
        const Token* tok = nullptr;
        while ((tok = peek()))
        {
            std::string tokenValue = tok->value;

            if (tokenValue == "library" || tokenValue == "use")
            {
                skipUntil(";");
                continue;
            }

            if (tokenValue == "entity")
            {
                auto entity = parseEntity();
                root.children.push_back(std::move(entity));
            }

            else if (tokenValue == "architecture")
            {
                auto architecture = parseArch();
                root.children.push_back(std::move(architecture));
            }

            else
            {
                error("Unexpected '" + tokenValue + "' at top level. Expected 'entity' or 'architecture'.");
            }
        }

        return root;
    }

    // --------------------------------------------------------------------------------------------

    [[nodiscard]]
    ASTRoot VHDLtoAST(Tokenizer& tokenizer)
    {
        ParseContext ctx(tokenizer);
        return ctx.parse();
    }

} // namespace Pulse::Parser
