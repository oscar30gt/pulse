#include "parser_impl.h"

#include <utility>

namespace Pulse::Parser
{
    // ---- Declarative parts ----------------------------------------------------------------------

    std::vector<DeclarationPtr> Parser::parseDeclarativePart(Region region)
    {
        std::vector<DeclarationPtr> declarations;
        while (atDeclaration(region))
            parseDeclaration(declarations);
        return declarations;
    }

    bool Parser::atDeclaration(Region region) const
    {
        if (tokens.atAny({ "type", "subtype", "constant", "function", "procedure", "pure", "impure", "alias", "attribute", "use" }))
            return true;
        if (region == Region::Architecture)
            return tokens.atAny({ "signal", "component" });
        return tokens.at("variable");
    }

    /// Called only when atDeclaration() holds, so the dispatch below always consumes the first keyword.
    void Parser::parseDeclaration(std::vector<DeclarationPtr>& out)
    {
        if (tokens.at("type"))           out.push_back(parseTypeDeclaration());
        else if (tokens.at("subtype"))   out.push_back(parseSubtypeDeclaration());
        else if (tokens.at("signal"))    appendAll(out, parseSignalDeclaration());
        else if (tokens.at("constant"))  appendAll(out, parseConstantDeclaration());
        else if (tokens.at("variable"))  appendAll(out, parseVariableDeclaration());
        else if (tokens.at("component")) out.push_back(parseComponentDeclaration());
        else if (tokens.at("alias"))     out.push_back(parseAliasDeclaration());
        else if (tokens.at("attribute")) out.push_back(parseAttribute());
        else if (tokens.at("use"))       out.push_back(parseUseClause());
        else                             out.push_back(parseSubprogram());
    }

    // ---- Objects --------------------------------------------------------------------------------

    /// `signal names : subtype [:= value] ;`
    std::vector<std::unique_ptr<SignalDeclaration>> Parser::parseSignalDeclaration()
    {
        tokens.expect("signal");
        std::vector<const Token*> names = parseIdentifierList();
        tokens.expect(":");
        std::unique_ptr<TypeSpec> type = parseTypeSpec();
        ExpressionPtr value = parseOptionalDefault();
        tokens.expect(";");

        return declareEach<SignalDeclaration>(names, [&](SignalDeclaration& signal)
        {
            signal.typeSpec = cloneOf(type);
            signal.initialValue = cloneOf(value);
        });
    }

    /// `constant names : subtype := value ;`
    std::vector<std::unique_ptr<ConstantDeclaration>> Parser::parseConstantDeclaration()
    {
        tokens.expect("constant");
        std::vector<const Token*> names = parseIdentifierList();
        tokens.expect(":");
        std::unique_ptr<TypeSpec> type = parseTypeSpec();
        tokens.expect(":=");
        ExpressionPtr value = parseExpression();
        tokens.expect(";");

        return declareEach<ConstantDeclaration>(names, [&](ConstantDeclaration& constant)
        {
            constant.typeSpec = cloneOf(type);
            constant.value = cloneOf(value);
        });
    }

    /// `variable names : subtype [:= value] ;`
    std::vector<std::unique_ptr<VariableDeclaration>> Parser::parseVariableDeclaration()
    {
        tokens.expect("variable");
        std::vector<const Token*> names = parseIdentifierList();
        tokens.expect(":");
        std::unique_ptr<TypeSpec> type = parseTypeSpec();
        ExpressionPtr value = parseOptionalDefault();
        tokens.expect(";");

        return declareEach<VariableDeclaration>(names, [&](VariableDeclaration& variable)
        {
            variable.typeSpec = cloneOf(type);
            variable.initialValue = cloneOf(value);
        });
    }

    // ---- Components -----------------------------------------------------------------------------

    /// `component name [is] [generic (...);] [port (...);] end component [name] ;`
    std::unique_ptr<ComponentDeclaration> Parser::parseComponentDeclaration()
    {
        auto component = node<ComponentDeclaration>(tokens.expect("component"));
        component->name = tokens.expectIdentifier()->value;
        tokens.accept("is");
        component->generics = parseOptionalGenericClause();
        component->ports = parseOptionalPortClause();
        expectEnd("component", component->name);
        return component;
    }

    // ---- Aliases --------------------------------------------------------------------------------

    /// `alias designator [: subtype] is name [signature] ;`
    std::unique_ptr<AliasDeclaration> Parser::parseAliasDeclaration()
    {
        auto alias = node<AliasDeclaration>(tokens.expect("alias"));
        alias->name = parseDesignator();
        if (tokens.accept(":"))
            alias->subtype = parseTypeSpec();
        tokens.expect("is");
        alias->target = parseName();
        if (tokens.at("["))
            alias->signature = parseSignature();
        tokens.expect(";");
        return alias;
    }

    std::string Parser::parseDesignator()
    {
        if (tokens.atType(TokenType::Identifier) || tokens.atType(TokenType::CharacterLiteral))
            return tokens.next()->value;
        if (tokens.atType(TokenType::StringLiteral))
            return operatorSymbolName(*tokens.next());
        tokens.expected("a name");
    }

    // ---- Attributes -----------------------------------------------------------------------------

    DeclarationPtr Parser::parseAttribute()
    {
        const Token* start = tokens.expect("attribute");
        const Token* name = tokens.expectIdentifier();
        if (tokens.at(":"))
            return finishAttributeDeclaration(start, name);
        return finishAttributeSpecification(start, name);
    }

    /// `attribute name : type_mark ;` from the colon.
    std::unique_ptr<AttributeDeclaration> Parser::finishAttributeDeclaration(const Token* start, const Token* name)
    {
        auto declaration = node<AttributeDeclaration>(start);
        declaration->name = name->value;
        tokens.expect(":");
        declaration->typeMark = parseTypeMark();
        tokens.expect(";");
        return declaration;
    }

    /// `attribute name of entities : class is value ;` from `of`.
    std::unique_ptr<AttributeSpecification> Parser::finishAttributeSpecification(const Token* start, const Token* name)
    {
        auto specification = node<AttributeSpecification>(start);
        specification->attributeName = name->value;
        tokens.expect("of");
        specification->entities = parseEntityNameList();
        tokens.expect(":");
        specification->entityClass = parseEntityClass();
        tokens.expect("is");
        specification->value = parseExpression();
        tokens.expect(";");
        return specification;
    }

    /// `others`, `all`, or `designator {, designator}`.
    std::vector<ExpressionPtr> Parser::parseEntityNameList()
    {
        std::vector<ExpressionPtr> entities;
        if (const Token* others = tokens.accept("others"))
            entities.push_back(node<OthersExpr>(others));
        else if (const Token* all = tokens.accept("all"))
            entities.push_back(node<AllExpr>(all));
        else
            parseSeparated(",", [&]
            {
                auto entity = node<SymbolExpr>(tokens.peek());
                entity->name = parseDesignator();
                entities.push_back(std::move(entity));
            });
        return entities;
    }

    EntityClass Parser::parseEntityClass()
    {
        static const std::pair<std::string_view, EntityClass> classes[] = {
            { "entity", EntityClass::Entity },       { "architecture", EntityClass::Architecture },
            { "configuration", EntityClass::Configuration }, { "procedure", EntityClass::Procedure },
            { "function", EntityClass::Function },   { "package", EntityClass::Package },
            { "type", EntityClass::Type },           { "subtype", EntityClass::Subtype },
            { "constant", EntityClass::Constant },   { "signal", EntityClass::Signal },
            { "variable", EntityClass::Variable },   { "component", EntityClass::Component },
            { "label", EntityClass::Label },         { "literal", EntityClass::Literal },
            { "units", EntityClass::Units },         { "group", EntityClass::Group },
            { "file", EntityClass::File },           { "property", EntityClass::Property },
            { "sequence", EntityClass::Sequence },
        };

        for (const auto& [word, entityClass] : classes)
            if (tokens.accept(word))
                return entityClass;
        tokens.expected("an entity class");
    }

} // namespace Pulse::Parser
