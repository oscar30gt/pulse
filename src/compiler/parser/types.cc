#include "parser_impl.h"

namespace Pulse::Parser
{
    // ---- Type and subtype declarations ----------------------------------------------------------

    /// `type name is definition ;`
    std::unique_ptr<TypeDeclaration> Parser::parseTypeDeclaration()
    {
        auto declaration = node<TypeDeclaration>(tokens.expect("type"));
        declaration->name = tokens.expectIdentifier()->value;
        tokens.expect("is");
        declaration->definition = parseTypeDefinition(declaration->name);
        tokens.expect(";");
        return declaration;
    }

    /// `subtype name is subtype_indication ;`
    std::unique_ptr<SubtypeDeclaration> Parser::parseSubtypeDeclaration()
    {
        auto declaration = node<SubtypeDeclaration>(tokens.expect("subtype"));
        declaration->name = tokens.expectIdentifier()->value;
        tokens.expect("is");
        declaration->baseType = parseTypeSpec();
        tokens.expect(";");
        return declaration;
    }

    TypeDefinitionPtr Parser::parseTypeDefinition(const std::string& typeName)
    {
        if (tokens.at("("))      return parseEnumeration();
        if (tokens.at("range"))  return parseRangeTypeDefinition(typeName);
        if (tokens.at("array"))  return parseArrayType();
        if (tokens.at("record")) return parseRecord(typeName);
        tokens.expected("a type definition");
    }

    // ---- Enumerations ---------------------------------------------------------------------------

    /// `( literal {, literal} )`
    std::unique_ptr<EnumeratedTypeDefinition> Parser::parseEnumeration()
    {
        auto enumeration = node<EnumeratedTypeDefinition>(tokens.peek());
        parseParenthesized([&] { enumeration->literals.push_back(parseEnumerationLiteral()); });
        return enumeration;
    }

    std::string Parser::parseEnumerationLiteral()
    {
        if (tokens.atType(TokenType::Identifier) || tokens.atType(TokenType::CharacterLiteral))
            return tokens.next()->value;
        tokens.expected("an enumeration literal");
    }

    // ---- Integer, real and physical types -------------------------------------------------------

    /// `range r` or `range r units ... end units [name]`
    TypeDefinitionPtr Parser::parseRangeTypeDefinition(const std::string& typeName)
    {
        const Token* start = tokens.peek();
        ExpressionPtr range = parseRangeConstraint();
        if (tokens.at("units"))
            return finishPhysicalType(start, std::move(range), typeName);

        auto numeric = node<NumericTypeDefinition>(start);
        numeric->range = std::move(range);
        return numeric;
    }

    /// `units base ; {secondary ;} end units [name]`
    std::unique_ptr<PhysicalTypeDefinition> Parser::finishPhysicalType(const Token* start, ExpressionPtr range, const std::string& typeName)
    {
        auto physical = node<PhysicalTypeDefinition>(start);
        physical->range = std::move(range);
        tokens.expect("units");
        physical->units.push_back(parseBaseUnit());
        while (!tokens.at("end"))
            physical->units.push_back(parseSecondaryUnit());
        tokens.expect("end");
        tokens.expect("units");
        acceptClosingName(typeName);
        return physical;
    }

    std::unique_ptr<UnitDeclaration> Parser::parseBaseUnit()
    {
        const Token* name = tokens.expectIdentifier();
        auto unit = node<UnitDeclaration>(name);
        unit->name = name->value;
        tokens.expect(";");
        return unit;
    }

    std::unique_ptr<UnitDeclaration> Parser::parseSecondaryUnit()
    {
        const Token* name = tokens.expectIdentifier();
        auto unit = node<UnitDeclaration>(name);
        unit->name = name->value;
        tokens.expect("=");
        if (tokens.atType(TokenType::NumericLiteral))
            unit->multiplier = parseAbstractLiteral();
        unit->ofUnit = tokens.expectIdentifier()->value;
        tokens.expect(";");
        return unit;
    }

    // ---- Arrays ---------------------------------------------------------------------------------

    /// `array ( index {, index} ) of subtype_indication`: every index is `type_mark range <>` (unconstrained)
    /// or a discrete range (constrained).
    TypeDefinitionPtr Parser::parseArrayType()
    {
        const Token* start = tokens.expect("array");
        tokens.expect("(");
        if (atUnconstrainedIndex())
            return finishUnconstrainedArray(start);
        return finishConstrainedArray(start);
    }

    bool Parser::atUnconstrainedIndex() const
    {
        return tokens.atType(TokenType::Identifier) && tokens.at("range", 1) && tokens.at("<>", 2);
    }

    std::unique_ptr<UnconstrainedArrayTypeDefinition> Parser::finishUnconstrainedArray(const Token* start)
    {
        auto array = node<UnconstrainedArrayTypeDefinition>(start);
        parseSeparated(",", [&] { array->indexTypeMarks.push_back(parseUnconstrainedIndex()); });
        tokens.expect(")");
        array->elementType = parseArrayElementType();
        return array;
    }

    std::unique_ptr<ArrayTypeDefinition> Parser::finishConstrainedArray(const Token* start)
    {
        auto array = node<ArrayTypeDefinition>(start);
        parseSeparated(",", [&] { array->indexRanges.push_back(parseDiscreteRange()); });
        tokens.expect(")");
        array->elementType = parseArrayElementType();
        return array;
    }

    std::string Parser::parseUnconstrainedIndex()
    {
        std::string typeMark = tokens.expectIdentifier()->value;
        tokens.expect("range");
        tokens.expect("<>");
        return typeMark;
    }

    std::unique_ptr<TypeSpec> Parser::parseArrayElementType()
    {
        tokens.expect("of");
        return parseTypeSpec();
    }

    // ---- Records --------------------------------------------------------------------------------

    /// `record fields {fields} end record [name]`
    std::unique_ptr<RecordTypeDefinition> Parser::parseRecord(const std::string& typeName)
    {
        auto record = node<RecordTypeDefinition>(tokens.expect("record"));
        do appendAll(record->fields, parseRecordFieldGroup());
        while (!tokens.at("end"));
        tokens.expect("end");
        tokens.expect("record");
        acceptClosingName(typeName);
        return record;
    }

    /// `names : subtype_indication ;`
    std::vector<std::unique_ptr<RecordField>> Parser::parseRecordFieldGroup()
    {
        std::vector<const Token*> names = parseIdentifierList();
        tokens.expect(":");
        std::unique_ptr<TypeSpec> type = parseTypeSpec();
        tokens.expect(";");

        return declareEach<RecordField>(names, [&](RecordField& field) { field.type = cloneOf(type); });
    }

    // ---- Subtype indications --------------------------------------------------------------------

    /// `[resolution] type_mark [constraint]`
    std::unique_ptr<TypeSpec> Parser::parseTypeSpec()
    {
        auto spec = node<TypeSpec>(tokens.peek());
        spec->resolution = parseOptionalResolution();
        spec->typeName = tokens.expectIdentifier()->value;
        parseOptionalConstraint(*spec);
        return spec;
    }

    std::unique_ptr<TypeSpec> Parser::parseTypeMark()
    {
        const Token* name = tokens.expectIdentifier();
        auto spec = node<TypeSpec>(name);
        spec->typeName = name->value;
        return spec;
    }

    /// A resolution function name (two identifiers in a row: `resolved std_logic`) or an element
    /// resolution in parentheses (`(resolved) std_logic_vector`).
    ExpressionPtr Parser::parseOptionalResolution()
    {
        if (tokens.at("("))
            return parseElementResolution();
        if (tokens.atType(TokenType::Identifier) && tokens.atType(TokenType::Identifier, 1))
            return parseSymbol();
        return nullptr;
    }

    ExpressionPtr Parser::parseElementResolution()
    {
        NestingGuard guard(*this);
        auto resolution = node<ElementResolutionExpr>(tokens.expect("("));
        resolution->resolution = tokens.at("(") ? parseElementResolution() : parseSymbol();
        tokens.expect(")");
        return resolution;
    }

    /// `( discrete_range {, discrete_range} )` or `range r`.
    void Parser::parseOptionalConstraint(TypeSpec& spec)
    {
        if (tokens.at("("))
            spec.args = parseIndexConstraint();
        else if (tokens.at("range"))
            spec.range = parseRangeConstraint();
    }

    std::vector<ExpressionPtr> Parser::parseIndexConstraint()
    {
        std::vector<ExpressionPtr> ranges;
        parseParenthesized([&] { ranges.push_back(parseActual()); });
        return ranges;
    }

    ExpressionPtr Parser::parseRangeConstraint()
    {
        tokens.expect("range");
        return parseRange();
    }

} // namespace Pulse::Parser
