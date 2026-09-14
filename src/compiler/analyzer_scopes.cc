#include "analyzer_internal.h"
#include <algorithm>

namespace Pulse::Parser
{
    void AnalyzerContext::pushScope()
    {
        m_scopes.emplace_back();
    }

    void AnalyzerContext::popScope()
    {
        if (m_scopes.empty())
        {
            throw std::runtime_error("Attempted to pop scope from an empty stack.");
        }
        m_scopes.pop_back();
    }

    void AnalyzerContext::declareTag(
        const std::string& tag,
        const ASTNode& node
    )
    {
        if (m_scopes.empty())
        {
            throw std::runtime_error("No active scope to declare a tag.");
        }
        Scope& currentScope = m_scopes.back();
        if (std::find(currentScope.tags.begin(), currentScope.tags.end(), tag) != currentScope.tags.end())
        {
            throw ast_semantic_error("Tag '" + tag + "' already exists in the current scope.", node.source);
        }
        currentScope.tags.push_back(tag);
    }

    void AnalyzerContext::declareSymbol(
        const std::string& name,
        SymbolKind kind,
        const TypeSpec& type,
        const ASTNode& node
    )
    {
        if (m_scopes.empty())
        {
            throw std::runtime_error("No active scope to declare a symbol.");
        }
        Scope& currentScope = m_scopes.back();
        if (currentScope.symbols.find(name) != currentScope.symbols.end())
        {
            throw ast_semantic_error("Symbol '" + name + "' already exists in the current scope.", node.source);
        }
        currentScope.symbols.try_emplace(name, kind, cloneTypeSpec(type), node.source);
    }

    const SymbolInfo& AnalyzerContext::getSymbol(const std::string& name, const ASTNode& node) const
    {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it)
        {
            auto found = it->symbols.find(name);
            if (found != it->symbols.end())
            {
                return found->second;
            }
        }
        throw ast_semantic_error("Symbol '" + name + "' not found in any active scope.", node.source);
    }

    bool AnalyzerContext::hasSymbol(const std::string& name) const
    {
        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it)
        {
            if (it->symbols.find(name) != it->symbols.end())
            {
                return true;
            }
        }
        return false;
    }

} // namespace Pulse::Parser