#include "parser_impl.h"

namespace Pulse::Parser
{
    // ---- Names: prefix { suffix } ---------------------------------------------------------------

    ExpressionPtr Parser::parseName()
    {
        return parseNameSuffixes(parseNamePrefix());
    }

    /// An identifier, an operator symbol, or (VHDL-2008) an external name.
    ExpressionPtr Parser::parseNamePrefix()
    {
        if (tokens.at("<<"))
            return parseExternalName();
        if (tokens.atType(TokenType::StringLiteral))
            return parseOperatorSymbol();
        return parseSymbol();
    }

    ExpressionPtr Parser::parseSymbol()
    {
        const Token* name = tokens.expectIdentifier();
        auto symbol = node<SymbolExpr>(name);
        symbol->name = name->value;
        return symbol;
    }

    /// An overloaded operator used as a function name: `"and"(a, b)`.
    ExpressionPtr Parser::parseOperatorSymbol()
    {
        const Token* name = tokens.next();
        auto symbol = node<SymbolExpr>(name);
        symbol->name = operatorSymbolName(*name);
        return symbol;
    }

    /// Every suffix consumes at least one token, so the loop always ends.
    ExpressionPtr Parser::parseNameSuffixes(ExpressionPtr name)
    {
        ChainScope chain(*this);
        while (true)
        {
            if (tokens.at(".") || tokens.at("(") || tokens.at("'") || tokens.at("["))
                chain.extend();

            if (tokens.at("."))
                name = parseSelectedSuffix(std::move(name));
            else if (tokens.at("("))
                name = parseCallSuffix(std::move(name));
            else if (tokens.at("'"))
                name = parseTickSuffix(std::move(name), nullptr);
            else if (tokens.at("["))
            {
                // `name[signature]'attribute`. Without the tick the brackets belong to the caller (an alias
                // signature), so the cursor goes back to them.
                const size_t start = tokens.position();
                std::unique_ptr<SignatureExpr> signature = parseSignature();
                if (!tokens.at("'"))
                {
                    tokens.seek(start);
                    return name;
                }
                name = parseTickSuffix(std::move(name), std::move(signature));
            }
            else
                return name;
        }
    }

    /// `.field`, `.all`, `.'c'` or `."op"`
    ExpressionPtr Parser::parseSelectedSuffix(ExpressionPtr prefix)
    {
        tokens.expect(".");
        auto selected = std::make_unique<FieldAccessExpr>();
        selected->source = prefix->source;
        selected->target = std::move(prefix);

        if (tokens.accept("all"))
            selected->fieldName = "all";
        else if (tokens.atType(TokenType::Identifier) || tokens.atType(TokenType::CharacterLiteral))
            selected->fieldName = tokens.next()->value;
        else if (tokens.atType(TokenType::StringLiteral))
            selected->fieldName = operatorSymbolName(*tokens.next());
        else
            tokens.expected("a name after '.'");
        return selected;
    }

    /// `( association {, association} )`: an index, a slice, a call or a conversion.
    ExpressionPtr Parser::parseCallSuffix(ExpressionPtr callee)
    {
        auto call = std::make_unique<FunctionCallExpr>();
        call->source = callee->source;
        call->callee = std::move(callee);
        parseParenthesized([&] { call->arguments.push_back(parseAssociation()); });
        return call;
    }

    /// `'attribute` or `'( ... )` (a qualified expression).
    ExpressionPtr Parser::parseTickSuffix(ExpressionPtr prefix, std::unique_ptr<SignatureExpr> signature)
    {
        tokens.expect("'");
        if (!signature && tokens.at("("))
            return parseQualifiedSuffix(std::move(prefix));

        auto attribute = std::make_unique<AttributeExpr>();
        attribute->source = prefix->source;
        attribute->prefix = std::move(prefix);
        attribute->signature = std::move(signature);
        attribute->attributeName = parseAttributeDesignator();
        return attribute;
    }

    /// `type_mark'( expression )` or `type_mark'aggregate`, from the parenthesis. The prefix must be a type mark.
    ExpressionPtr Parser::parseQualifiedSuffix(ExpressionPtr prefix)
    {
        auto* typeMark = dynamic_cast<const SymbolExpr*>(prefix.get());
        if (!typeMark)
            tokens.expected("an attribute name");

        auto type = std::make_unique<TypeSpec>();
        type->source = prefix->source;
        type->typeName = typeMark->name;

        auto qualified = std::make_unique<QualifiedExpr>();
        qualified->source = prefix->source;
        qualified->typeMark = std::move(type);
        qualified->operand = parseAggregateOrParenthesized();
        return qualified;
    }

    /// An identifier, or the reserved words that name predefined attributes (`'range`, `'subtype`).
    std::string Parser::parseAttributeDesignator()
    {
        if (tokens.atType(TokenType::Identifier) || tokens.atAny({ "range", "subtype" }))
            return tokens.next()->value;
        tokens.expected("an attribute name");
    }

    /// `[ [type_mark {, type_mark}] [return type_mark] ]`
    std::unique_ptr<SignatureExpr> Parser::parseSignature()
    {
        auto signature = node<SignatureExpr>(tokens.expect("["));
        if (tokens.atType(TokenType::Identifier))
            parseSeparated(",", [&] { signature->parameters.push_back(parseTypeMark()); });
        if (tokens.accept("return"))
            signature->returnType = parseTypeMark();
        tokens.expect("]");
        return signature;
    }

    // ---- External names (VHDL-2008) -------------------------------------------------------------

    /// `<< constant|signal|variable path : subtype >>`
    ExpressionPtr Parser::parseExternalName()
    {
        auto external = node<ExternalNameExpr>(tokens.expect("<<"));
        external->objectClass = parseExternalObjectClass();
        external->path = parseExternalPath();
        tokens.expect(":");
        external->subtype = parseTypeSpec();
        tokens.expect(">>");
        return external;
    }

    ExternalObjectClass Parser::parseExternalObjectClass()
    {
        if (tokens.accept("constant")) return ExternalObjectClass::Constant;
        if (tokens.accept("signal"))   return ExternalObjectClass::Signal;
        if (tokens.accept("variable")) return ExternalObjectClass::Variable;
        tokens.expected("'constant', 'signal' or 'variable'");
    }

    /// The path up to the colon, its tokens joined: `.tb.dut.count`, `@lib.pkg.obj`, `^.^.x`.
    std::string Parser::parseExternalPath()
    {
        std::string path;
        do
        {
            if (!tokens.atType(TokenType::Identifier) && !tokens.atAny({ ".", "@", "^" }))
                tokens.expected("an external path");
            path += tokens.next()->value;
        }
        while (!tokens.at(":"));
        return path;
    }

} // namespace Pulse::Parser
