#include "parser_impl.h"

namespace Pulse::Parser
{
    namespace
    {
        /// Deepest nesting of parentheses, statements, element resolutions and subprogram bodies. Real designs
        /// stay far below it; the bound only keeps hostile input from overflowing the stack.
        constexpr int maxNesting = 100;

        /// Longest chain of operators, selections or alternatives along one path of an expression. Beyond it a tree is deeper than
        /// the recursive passes over it (clone, print, analysis) can safely go.
        constexpr size_t maxTreeDepth = 2000;
    } // anonymous namespace

    Parser::Parser(Tokenizer& tokenizer)
        : tokens(tokenizer)
    { }

    // ---- Entry points ---------------------------------------------------------------------------

    ASTRoot Parser::parseFile()
    {
        ASTRoot root;
        root.source = locationOf(tokens.peek());
        while (!tokens.atEnd())
            root.children.push_back(parseDesignUnit());
        return root;
    }

    std::vector<DeclarationPtr> Parser::parseStandaloneDeclarations()
    {
        std::vector<DeclarationPtr> declarations = parseDeclarativePart(Region::Architecture);
        if (!tokens.atEnd())
            tokens.expected("a declaration");
        return declarations;
    }

    ASTRoot VHDLtoAST(Tokenizer& tokenizer)
    {
        Parser parser(tokenizer);
        return parser.parseFile();
    }

    std::vector<DeclarationPtr> parseDeclarations(Tokenizer& tokenizer)
    {
        Parser parser(tokenizer);
        return parser.parseStandaloneDeclarations();
    }

    // ---- Nesting --------------------------------------------------------------------------------

    Parser::NestingGuard::NestingGuard(Parser& parser)
        : m_parser(parser)
    {
        if (++m_parser.depth > maxNesting)
        {
            --m_parser.depth;
            m_parser.tokens.fail("Nesting is too deep (more than " + std::to_string(maxNesting) + " levels)");
        }
    }

    Parser::NestingGuard::~NestingGuard()
    {
        --m_parser.depth;
    }

    void Parser::ChainScope::extend()
    {
        ++m_links;
        if (++m_parser.treeDepth > maxTreeDepth)
            m_parser.tokens.fail("This expression is too long: more than " + std::to_string(maxTreeDepth)
                                 + " chained operators, selections or alternatives");
    }

    // ---- Labels and closing names ---------------------------------------------------------------

    std::string Parser::parseOptionalLabel()
    {
        if (!tokens.atType(TokenType::Identifier) || !tokens.at(":", 1))
            return "";

        std::string label = tokens.next()->value;
        tokens.expect(":");
        return label;
    }

    void Parser::expectEnd(std::string_view kind, const std::string& name)
    {
        tokens.expect("end");
        tokens.expect(kind);
        finishEnd(name);
    }

    void Parser::expectEndOptionalKind(std::string_view kind, const std::string& name)
    {
        tokens.expect("end");
        tokens.accept(kind);
        finishEnd(name);
    }

    void Parser::finishEnd(const std::string& name)
    {
        acceptClosingName(name);
        tokens.expect(";");
    }

    void Parser::acceptClosingName(const std::string& name)
    {
        const bool operatorSymbol = !name.empty() && name.front() == '"';
        if (!tokens.atType(operatorSymbol ? TokenType::StringLiteral : TokenType::Identifier))
            return;

        const Token* closing = tokens.next();
        const std::string closingName = operatorSymbol ? operatorSymbolName(*closing) : closing->value;

        if (name.empty())
            tokens.failAt(closing, "Closing name " + describe(closing) + " given, but the opening has no name");
        if (closingName != name)
            tokens.failAt(closing, "Closing name " + describe(closing) + " does not match '" + name + "'");
    }

} // namespace Pulse::Parser
