#include "printer.h"

#include <iostream>

namespace Pulse::Parser::AstPrint
{
    std::string pad(int indent)
    {
        return std::string(static_cast<size_t>(indent > 0 ? indent : 0) * 4, ' ');
    }

    void printLine(int indent, const char* kind, const std::string& value, const char* valueColor)
    {
        std::cout << pad(indent) << KEYWORD << kind;
        if (!value.empty())
            std::cout << ": " << valueColor << value;
        std::cout << RESET << "\n";
    }

    int printSection(int indent, const std::string& label, size_t count)
    {
        std::cout << pad(indent) << KEYWORD << label;
        if (count > 0)
            std::cout << " (" << VALUE << count << KEYWORD << ")";
        std::cout << ":" << RESET << "\n";
        return indent + 1;
    }

    void printNull(int indent, const char* what)
    {
        std::cout << pad(indent) << DIM << "<" << what << ">" << RESET << "\n";
    }

    void printChild(const ASTNode* node, int indent)
    {
        if (node)
            node->print(indent);
        else
            printNull(indent, "none");
    }

    void printField(int indent, const std::string& label, const ASTNode* node)
    {
        printChild(node, printSection(indent, label));
    }

    void printOptionalField(int indent, const std::string& label, const ASTNode* node)
    {
        if (node)
            printField(indent, label, node);
    }

    void printStatementLine(int indent, const char* kind, const Statement& statement)
    {
        printLine(indent, kind, statement.label);
    }

    std::string quoted(const std::string& text)
    {
        return "\"" + text + "\"";
    }

} // namespace Pulse::Parser::AstPrint

namespace Pulse::Parser
{
    void ASTRoot::print(int indent) const
    {
        using namespace AstPrint;

        std::cout << "\n" << DIM << std::string(60, '=') << "\n";
        std::cout << "VHDL AST" << "\n";
        std::cout << std::string(60, '=') << RESET << "\n\n";

        if (children.empty())
        {
            std::cout << pad(indent) << DIM << "<empty AST>" << RESET << "\n\n";
            return;
        }

        printLine(indent, "ROOT");
        for (const auto& child : children)
            printChild(child.get(), indent + 1);

        std::cout << "\n" << DIM << std::string(60, '=') << RESET << "\n\n";
    }

} // namespace Pulse::Parser
