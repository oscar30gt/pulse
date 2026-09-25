#ifndef PULSE_VHDL_AST_NODE_H
#define PULSE_VHDL_AST_NODE_H

#include "diagnostics.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Pulse::Parser
{
    // --------------------------------------------------------------------------------------------
    // Node base classes
    //
    // Every AST node derives from ASTNode, and every node is a real node: even the small helpers
    // (an `if` branch, a `case` alternative, a port ...) carry a source location and can be cloned
    // and printed on their own. Children are owned through unique_ptr, so nodes cannot be copied
    // by accident; clone() is the one way to duplicate a subtree.
    //
    // The families containers refer to are Expression, Statement (concurrent or sequential),
    // Declaration, TypeDefinition and DesignUnit. The concrete nodes live in expressions.h,
    // statements.h and declarations.h.
    // --------------------------------------------------------------------------------------------

    /// Base class for all AST nodes to allow proper polymorphism without slicing.
    struct ASTNode
    {
        SourceLocation source;  /// Location of the node in the source file

        virtual ~ASTNode() = default;

        /// Creates a deep copy of the node and everything below it.
        /// @returns Unique pointer to a copy with the same dynamic type as this node.
        virtual std::unique_ptr<ASTNode> clone() const = 0;

        /// Prints the node and its children to stdout in a human-readable, colored format.
        /// This is primarily for debugging purposes.
        /// @param indent Nesting level of the node; every unit is 4 spaces.
        virtual void print(int indent = 0) const = 0;
    };

    /// Declares the members every concrete node implements (`clone` in clone*.cc, `print` in printer*.cc).
    #define PULSE_AST_NODE_OVERRIDES                                              \
        std::unique_ptr<ASTNode> clone() const override;                \
        void print(int indent = 0) const override;

    /// Base class for all evaluable expressions.
    struct Expression : ASTNode
    {
        /// The child a long chain of expressions nests through: the left operand of a chain of additions, the prefix of a chain
        /// of selections `a.b.c.d`, the alternative of a chain of `when ... else`; null for every other node. It exists so that
        /// such a chain (which the parser builds in a loop, however long it is) can be destroyed without recursion.
        virtual std::unique_ptr<Expression>* spineChild() { return nullptr; }
    };
    using ExpressionPtr = std::unique_ptr<Expression>;      /// Shorthand for a unique pointer to an Expression.

    /// Destroys the chain that starts at `head` one node at a time. A node whose destructor holds a spine child calls this,
    /// so freeing a chain of 100 000 additions takes a loop, not 100 000 nested destructor calls.
    void unlinkSpine(ExpressionPtr& head);

    /// Base class for all statements (concurrent or sequential).
    struct Statement : ASTNode
    {
        std::string label;   /// Label in front of the statement (`lbl : ...`), empty when it has none
    };
    using StatementPtr = std::unique_ptr<Statement>;

    struct Declaration : ASTNode { };                       /// Base class for everything declared in a declarative part.
    using DeclarationPtr = std::unique_ptr<Declaration>;

    struct TypeDefinition : ASTNode { };                    /// Base class for the right-hand side of `type <name> is ...;`.
    using TypeDefinitionPtr = std::unique_ptr<TypeDefinition>;

    /// Base class for the top-level units: entities and architectures.
    struct DesignUnit : ASTNode
    {
        std::vector<DeclarationPtr> context;    /// Context clause in front of the unit: `library` and `use` clauses, in order
    };
    using DesignUnitPtr = std::unique_ptr<DesignUnit>;

    // --------------------------------------------------------------------------------------------
    // Cloning helpers
    // --------------------------------------------------------------------------------------------

    /// Deep-copies an owned child, keeping its static type. Null stays null.
    template <typename T>
    std::unique_ptr<T> cloneOf(const std::unique_ptr<T>& node)
    {
        if (!node)
            return nullptr;
        // clone() preserves the dynamic type, so the copy really is a T.
        return std::unique_ptr<T>(static_cast<T*>(node->clone().release()));
    }

    /// Deep-copies every element of a list of owned children.
    template <typename T>
    std::vector<std::unique_ptr<T>> cloneAll(const std::vector<std::unique_ptr<T>>& nodes)
    {
        std::vector<std::unique_ptr<T>> copy;
        copy.reserve(nodes.size());
        for (const auto& node : nodes)
            copy.push_back(cloneOf(node));
        return copy;
    }

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_NODE_H
