#include "parser_impl.h"

namespace Pulse::Parser
{
    // ---- Generic and port clauses ---------------------------------------------------------------

    std::vector<std::unique_ptr<GenericDeclaration>> Parser::parseOptionalGenericClause()
    {
        std::vector<std::unique_ptr<GenericDeclaration>> generics;
        if (!tokens.accept("generic"))
            return generics;

        tokens.expect("(");
        parseSeparated(";", [&] { appendAll(generics, parseGenericGroup()); });
        tokens.expect(")");
        tokens.expect(";");
        return generics;
    }

    std::vector<std::unique_ptr<PortDeclaration>> Parser::parseOptionalPortClause()
    {
        std::vector<std::unique_ptr<PortDeclaration>> ports;
        if (!tokens.accept("port"))
            return ports;

        tokens.expect("(");
        parseSeparated(";", [&] { appendAll(ports, parsePortGroup()); });
        tokens.expect(")");
        tokens.expect(";");
        return ports;
    }

    /// `names : subtype [:= default]`
    std::vector<std::unique_ptr<GenericDeclaration>> Parser::parseGenericGroup()
    {
        std::vector<const Token*> names = parseIdentifierList();
        tokens.expect(":");
        std::unique_ptr<TypeSpec> type = parseTypeSpec();
        ExpressionPtr defaultValue = parseOptionalDefault();

        return declareEach<GenericDeclaration>(names, [&](GenericDeclaration& generic)
        {
            generic.typeSpec = cloneOf(type);
            generic.defaultValue = cloneOf(defaultValue);
        });
    }

    /// `names : [mode] subtype [:= default]`
    std::vector<std::unique_ptr<PortDeclaration>> Parser::parsePortGroup()
    {
        std::vector<const Token*> names = parseIdentifierList();
        tokens.expect(":");
        PortMode mode = parseOptionalMode();
        std::unique_ptr<TypeSpec> type = parseTypeSpec();
        ExpressionPtr defaultValue = parseOptionalDefault();

        return declareEach<PortDeclaration>(names, [&](PortDeclaration& port)
        {
            port.mode = mode;
            port.typeSpec = cloneOf(type);
            port.defaultValue = cloneOf(defaultValue);
        });
    }

    // ---- Subprogram parameters ------------------------------------------------------------------

    std::vector<std::unique_ptr<ParameterDeclaration>> Parser::parseParameterList()
    {
        std::vector<std::unique_ptr<ParameterDeclaration>> parameters;
        tokens.expect("(");
        parseSeparated(";", [&] { appendAll(parameters, parseParameterGroup()); });
        tokens.expect(")");
        return parameters;
    }

    /// `[class] names : [mode] subtype [:= default]`
    std::vector<std::unique_ptr<ParameterDeclaration>> Parser::parseParameterGroup()
    {
        ParameterClass objectClass = parseParameterClass();
        std::vector<const Token*> names = parseIdentifierList();
        tokens.expect(":");
        PortMode mode = parseOptionalMode();
        std::unique_ptr<TypeSpec> type = parseTypeSpec();
        ExpressionPtr defaultValue = parseOptionalDefault();

        return declareEach<ParameterDeclaration>(names, [&](ParameterDeclaration& parameter)
        {
            parameter.objectClass = objectClass;
            parameter.mode = mode;
            parameter.typeSpec = cloneOf(type);
            parameter.defaultValue = cloneOf(defaultValue);
        });
    }

    ParameterClass Parser::parseParameterClass()
    {
        if (tokens.accept("constant")) return ParameterClass::Constant;
        if (tokens.accept("signal"))   return ParameterClass::Signal;
        if (tokens.accept("variable")) return ParameterClass::Variable;
        return ParameterClass::Unspecified;
    }

    // ---- Shared pieces --------------------------------------------------------------------------

    PortMode Parser::parseOptionalMode()
    {
        if (tokens.accept("in"))      return PortMode::In;
        if (tokens.accept("out"))     return PortMode::Out;
        if (tokens.accept("inout"))   return PortMode::InOut;
        return PortMode::In;
    }

    std::vector<const Token*> Parser::parseIdentifierList()
    {
        std::vector<const Token*> names;
        parseSeparated(",", [&] { names.push_back(tokens.expectIdentifier()); });
        return names;
    }

    ExpressionPtr Parser::parseOptionalDefault()
    {
        if (!tokens.accept(":="))
            return nullptr;
        return parseExpression();
    }

    // ---- Association lists (generic map, port map, call arguments) ------------------------------

    std::vector<ExpressionPtr> Parser::parseOptionalMap(std::string_view kind)
    {
        std::vector<ExpressionPtr> associations;
        if (!tokens.accept(kind))
            return associations;

        tokens.expect("map");
        parseParenthesized([&] { associations.push_back(parseAssociation()); });
        return associations;
    }

    ExpressionPtr Parser::parseAssociation()
    {
        ExpressionPtr first = parseActual();
        if (!tokens.accept("=>"))
            return first;

        auto association = std::make_unique<NamedAssociationExpr>();
        association->source = first->source;
        association->formal = std::move(first);
        association->actual = parseActual();
        return association;
    }

    ExpressionPtr Parser::parseActual()
    {
        if (const Token* open = tokens.accept("open"))
            return node<OpenExpr>(open);
        return parseDiscreteRange();
    }

} // namespace Pulse::Parser
