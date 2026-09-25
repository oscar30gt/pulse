#include "parser_impl.h"

namespace Pulse::Parser
{
    // ---- Design units ---------------------------------------------------------------------------

    DesignUnitPtr Parser::parseDesignUnit()
    {
        std::vector<DeclarationPtr> context = parseContextClause();
        DesignUnitPtr unit = parseLibraryUnit();
        unit->context = std::move(context);
        return unit;
    }

    DesignUnitPtr Parser::parseLibraryUnit()
    {
        if (tokens.at("entity"))
            return parseEntity();
        if (tokens.at("architecture"))
            return parseArchitecture();
        tokens.expected("'entity' or 'architecture'");
    }

    // ---- Context clauses ------------------------------------------------------------------------

    std::vector<DeclarationPtr> Parser::parseContextClause()
    {
        std::vector<DeclarationPtr> items;
        while (tokens.atAny({ "library", "use" }))
        {
            if (tokens.at("library"))
                items.push_back(parseLibraryClause());
            else
                items.push_back(parseUseClause());
        }
        return items;
    }

    std::unique_ptr<LibraryClause> Parser::parseLibraryClause()
    {
        auto clause = node<LibraryClause>(tokens.expect("library"));
        for (const Token* name : parseIdentifierList())
            clause->names.push_back(name->value);
        tokens.expect(";");
        return clause;
    }

    std::unique_ptr<UseClause> Parser::parseUseClause()
    {
        auto clause = node<UseClause>(tokens.expect("use"));
        parseSeparated(",", [&] { clause->names.push_back(parseSelectedName()); });
        tokens.expect(";");
        return clause;
    }

    ExpressionPtr Parser::parseSelectedName()
    {
        ExpressionPtr name = parseSymbol();
        do
        {
            auto selected = node<FieldAccessExpr>(tokens.expect("."));
            selected->target = std::move(name);
            selected->fieldName = tokens.accept("all") ? "all" : tokens.expectIdentifier()->value;
            name = std::move(selected);
        }
        while (tokens.at("."));
        return name;
    }

    // ---- Entity ---------------------------------------------------------------------------------

    std::unique_ptr<EntityDeclaration> Parser::parseEntity()
    {
        auto entity = node<EntityDeclaration>(tokens.expect("entity"));
        entity->name = tokens.expectIdentifier()->value;
        tokens.expect("is");
        entity->generics = parseOptionalGenericClause();
        entity->ports = parseOptionalPortClause();
        expectEndOptionalKind("entity", entity->name);
        return entity;
    }

    // ---- Architecture ---------------------------------------------------------------------------

    std::unique_ptr<ArchitectureDeclaration> Parser::parseArchitecture()
    {
        auto architecture = node<ArchitectureDeclaration>(tokens.expect("architecture"));
        architecture->name = tokens.expectIdentifier()->value;
        tokens.expect("of");
        architecture->entityName = tokens.expectIdentifier()->value;
        tokens.expect("is");
        architecture->declarations = parseDeclarativePart(Region::Architecture);
        tokens.expect("begin");
        architecture->body = parseConcurrentStatements();
        expectEndOptionalKind("architecture", architecture->name);
        return architecture;
    }

} // namespace Pulse::Parser
