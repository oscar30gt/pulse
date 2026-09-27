#ifndef PULSE_VHDL_AST_PRINTER_H
#define PULSE_VHDL_AST_PRINTER_H

#include "ast.h"

#include <string>
#include <vector>

/// Internal helpers for the human-readable AST dump (ASTNode::print). Every node prints itself in
/// printer_expressions.cc, printer_statements.cc or printer_declarations.cc using these primitives.
/// A node knows only its own nesting level, so the dump is plain indentation: 4 spaces per level.
namespace Pulse::Parser::AstPrint
{
    // ANSI colors
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* DIM     = "\033[90m";   // placeholders, frames
    constexpr const char* KEYWORD = "\033[1;36m"; // node kinds and section labels
    constexpr const char* IDENT   = "\033[32m";   // names
    constexpr const char* VALUE   = "\033[33m";   // literals, counts
    constexpr const char* TYPE    = "\033[35m";   // type names, directions

    /// Indentation of a nesting level: 4 spaces per unit.
    std::string pad(int indent);

    /// Prints "<pad>KIND[: value]" for a node or a property of a node.
    void printLine(int indent, const char* kind, const std::string& value = "", const char* valueColor = IDENT);

    /// Prints a section header "label (n):" and returns the indent its items are printed at.
    int printSection(int indent, const std::string& label, size_t count = 0);

    /// Prints a dimmed `<what>` placeholder for an absent (null) child.
    void printNull(int indent, const char* what);

    /// Prints a child node at `indent`, or a `<none>` placeholder when it is null.
    void printChild(const ASTNode* node, int indent);

    /// Prints a titled single child: the "label:" header followed by the node one level deeper.
    void printField(int indent, const std::string& label, const ASTNode* node);

    /// Prints a titled single child only when it is present (optional clauses).
    void printOptionalField(int indent, const std::string& label, const ASTNode* node);

    /// Prints "label (n):" followed by every item of a list one level deeper. Prints nothing for an empty list.
    template <typename Items>
    void printList(int indent, const std::string& label, const Items& items)
    {
        if (items.empty())
            return;
        int inner = printSection(indent, label, items.size());
        for (const auto& item : items)
            printChild(item.get(), inner);
    }

    /// The header line of a statement: its kind, followed by its label when it has one.
    void printStatementLine(int indent, const char* kind, const Statement& statement);

    /// Quotes a value for display.
    std::string quoted(const std::string& text);

} // namespace Pulse::Parser::AstPrint

#endif // PULSE_VHDL_AST_PRINTER_H
