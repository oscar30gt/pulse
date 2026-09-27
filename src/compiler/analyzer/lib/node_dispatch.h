#ifndef PULSE_COMPILER_NODE_DISPATCH_H
#define PULSE_COMPILER_NODE_DISPATCH_H

#include <functional>
#include <typeindex>
#include <unordered_map>

namespace Pulse::Parser
{
    /// Maps the concrete class of an AST node to the member function that handles it.
    /// Supporting a new node kind means adding one `add(...)` line where the table is built.
    ///
    ///   NodeDispatch<Expression, SemanticType, const SemanticType*> table;
    ///   table.add(*this, &Analyzer::typeOfSymbol);      // SemanticType typeOfSymbol(const SymbolExpr&, const SemanticType*)
    template <class Base, class Result, class... Args>
    class NodeDispatch
    {
        using Handler = std::function<Result(const Base&, Args...)>;
        std::unordered_map<std::type_index, Handler> m_handlers;

    public:
        template <class Node, class Owner>
        void add(Owner& owner, Result (Owner::*handler)(const Node&, Args...))
        {
            m_handlers[std::type_index(typeid(Node))] = [&owner, handler](const Base& node, Args... args) -> Result
            {
                return (owner.*handler)(static_cast<const Node&>(node), args...);
            };
        }

        /// The handler registered for the node's concrete class, or nullptr.
        const Handler* find(const Base& node) const
        {
            auto found = m_handlers.find(std::type_index(typeid(node)));
            return found == m_handlers.end() ? nullptr : &found->second;
        }
    };

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_NODE_DISPATCH_H
