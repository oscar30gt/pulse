// types.test.cc — type definitions and subtype indications.

#include "parser_test_util.h"

using namespace ParserTest;

namespace
{
    std::vector<DeclarationPtr> declarationsOf(const std::string& text)
    {
        Tokenizer tokenizer(text);
        return parseDeclarations(tokenizer);
    }

    /// The definition of the single type declared by `text`.
    template <typename T>
    std::unique_ptr<T> definitionOf(const std::string& text)
    {
        auto declarations = declarationsOf(text);
        auto* declaration = dynamic_cast<TypeDeclaration*>(declarations.at(0).get());
        EXPECT_NE(declaration, nullptr);
        auto* definition = dynamic_cast<T*>(declaration->definition.get());
        EXPECT_NE(definition, nullptr) << "definition is not a " << typeid(T).name();
        declaration->definition.release();
        return std::unique_ptr<T>(definition);
    }

    /// The subtype indication of `subtype s is <indication>;`.
    std::unique_ptr<TypeSpec> subtypeOf(const std::string& indication)
    {
        auto declarations = declarationsOf("subtype s is " + indication + ";");
        return std::move(dynamic_cast<SubtypeDeclaration&>(*declarations.at(0)).baseType);
    }

    bool rejects(const std::string& text)
    {
        try
        {
            (void)declarationsOf(text);
        }
        catch (const ast_syntax_error&)
        {
            return true;
        }
        return false;
    }
}

// ===========================================================================
// 1. SCALAR TYPES
// ===========================================================================

TEST(ParserTypes, Enumeration)
{
    auto enumeration = definitionOf<EnumeratedTypeDefinition>("type state is (Idle, run, 'Z', '0');");
    EXPECT_EQ(enumeration->literals, (std::vector<std::string>{ "idle", "run", "'Z'", "'0'" }));
}

TEST(ParserTypes, EnumerationNeedsLiterals)
{
    EXPECT_TRUE(rejects("type t is ();"));
    EXPECT_TRUE(rejects("type t is (a, );"));
    EXPECT_TRUE(rejects("type t is (1, 2);"));
}

TEST(ParserTypes, IntegerAndRealRanges)
{
    EXPECT_EQ(render(definitionOf<NumericTypeDefinition>("type byte is range 0 to 255;")->range.get()), "(0 to 255)");
    EXPECT_EQ(render(definitionOf<NumericTypeDefinition>("type down is range 7 downto -1;")->range.get()), "(7 downto (- 1))");

    auto real = definitionOf<NumericTypeDefinition>("type r is range -1.0 to 1.0;");
    auto* range = as<BinaryOpExpr>(real->range.get());
    EXPECT_NE(dynamic_cast<const DoubleLiteralExpr*>(range->right.get()), nullptr);
}

TEST(ParserTypes, PhysicalType)
{
    auto physical = definitionOf<PhysicalTypeDefinition>(
        "type distance is range 0 to 1e9 units nm; um = 1000 nm; mm = 1_000 um; m = mm; end units distance;");
    EXPECT_EQ(render(physical->range.get()), "(0 to 1000000000)");
    ASSERT_EQ(physical->units.size(), 4u);

    EXPECT_EQ(physical->units[0]->name, "nm");
    EXPECT_EQ(physical->units[0]->multiplier, nullptr);
    EXPECT_TRUE(physical->units[0]->ofUnit.empty());

    EXPECT_EQ(physical->units[2]->name, "mm");
    EXPECT_EQ(render(physical->units[2]->multiplier.get()), "1000");
    EXPECT_EQ(physical->units[2]->ofUnit, "um");

    EXPECT_EQ(physical->units[3]->multiplier, nullptr) << "`m = mm` has no multiplier";
    EXPECT_EQ(physical->units[3]->ofUnit, "mm");
}

TEST(ParserTypes, PhysicalTypeGrammar)
{
    EXPECT_TRUE(rejects("type t is range 0 to 9 units a = 1 b; end units;")) << "the first unit is the base unit";
    EXPECT_TRUE(rejects("type t is range 0 to 9 units a; b = 2 a; end units u;")) << "closing name";
    EXPECT_TRUE(rejects("type t is range 0 to 9 units a; b = 2 a; end;"));
}

// ===========================================================================
// 2. COMPOSITE TYPES
// ===========================================================================

TEST(ParserTypes, ConstrainedArray)
{
    auto array = definitionOf<ArrayTypeDefinition>("type mem is array (0 to 15, 7 downto 0) of bit;");
    ASSERT_EQ(array->indexRanges.size(), 2u);
    EXPECT_EQ(render(array->indexRanges[1].get()), "(7 downto 0)");
    EXPECT_EQ(render(array->elementType.get()), "bit");
}

TEST(ParserTypes, ArrayIndexedByATypeOrASubtypeIndication)
{
    auto byType = definitionOf<ArrayTypeDefinition>("type t is array (state) of bit;");
    EXPECT_EQ(render(byType->indexRanges.at(0).get()), "state");

    auto bySubtype = definitionOf<ArrayTypeDefinition>("type t is array (natural range 0 to 3) of bit;");
    EXPECT_EQ(render(bySubtype->indexRanges.at(0).get()), "natural range (0 to 3)");
}

TEST(ParserTypes, UnconstrainedArray)
{
    auto array = definitionOf<UnconstrainedArrayTypeDefinition>(
        "type matrix is array (natural range <>, integer range <>) of std_logic_vector(3 downto 0);");
    EXPECT_EQ(array->indexTypeMarks, (std::vector<std::string>{ "natural", "integer" }));
    EXPECT_EQ(render(array->elementType.get()), "std_logic_vector((3 downto 0))");
}

TEST(ParserTypes, ArrayIndexesCannotBeMixed)
{
    EXPECT_TRUE(rejects("type t is array (natural range <>, 0 to 3) of bit;"));
    EXPECT_TRUE(rejects("type t is array (0 to 3, natural range <>) of bit;"));
    EXPECT_TRUE(rejects("type t is array (0 to 3) bit;"));
}

TEST(ParserTypes, Record)
{
    auto record = definitionOf<RecordTypeDefinition>(
        "type point is record x, y : integer; tag : string(1 to 4); end record point;");
    ASSERT_EQ(record->fields.size(), 3u);
    EXPECT_EQ(record->fields[1]->name, "y");
    EXPECT_EQ(render(record->fields[1]->type.get()), "integer");
    EXPECT_EQ(render(record->fields[2]->type.get()), "string((1 to 4))");
}

TEST(ParserTypes, RecordGrammar)
{
    EXPECT_TRUE(rejects("type r is record end record;")) << "a record has at least one field";
    EXPECT_TRUE(rejects("type r is record x : bit; end record q;"));
    EXPECT_TRUE(rejects("type r is record x : bit end record;"));
}

TEST(ParserTypes, UnsupportedTypeDefinitions)
{
    EXPECT_TRUE(rejects("type p is access integer;"));
    EXPECT_TRUE(rejects("type f is file of integer;"));
    EXPECT_TRUE(rejects("type t is protected end protected;"));
    EXPECT_TRUE(rejects("type t;")) << "incomplete type declarations";
}

// ===========================================================================
// 3. SUBTYPE INDICATIONS
// ===========================================================================

TEST(ParserTypes, SubtypeWithRangeConstraint)
{
    auto spec = subtypeOf("integer range 0 to 9");
    EXPECT_EQ(spec->typeName, "integer");
    EXPECT_EQ(render(spec->range.get()), "(0 to 9)");
    EXPECT_TRUE(spec->args.empty());
}

TEST(ParserTypes, SubtypeWithARangeAttribute)
{
    EXPECT_EQ(render(subtypeOf("integer range v'range").get()), "integer range v'range");
    EXPECT_EQ(render(subtypeOf("t(v'range)").get()), "t(v'range)");
}

TEST(ParserTypes, SubtypeWithIndexConstraints)
{
    EXPECT_EQ(render(subtypeOf("mem(0 to 3, open)").get()), "mem((0 to 3), open)");
    EXPECT_EQ(render(subtypeOf("t(n - 1 downto 0)").get()), "t(((n - 1) downto 0))");
}

TEST(ParserTypes, ResolutionFunction)
{
    auto spec = subtypeOf("resolved std_ulogic");
    EXPECT_EQ(spec->typeName, "std_ulogic");
    EXPECT_EQ(render(spec->resolution.get()), "resolved");
}

TEST(ParserTypes, ElementResolution)
{
    auto spec = subtypeOf("(resolved) std_ulogic_vector(7 downto 0)");
    EXPECT_EQ(spec->typeName, "std_ulogic_vector");
    ASSERT_NE(dynamic_cast<const ElementResolutionExpr*>(spec->resolution.get()), nullptr);
    EXPECT_EQ(render(spec->resolution.get()), "(resolved)");

    EXPECT_EQ(render(subtypeOf("((resolved)) matrix").get()), "((resolved)) matrix");
}

TEST(ParserTypes, SubtypeIndicationGrammar)
{
    EXPECT_TRUE(rejects("subtype s is ;"));
    EXPECT_TRUE(rejects("subtype s is () t;"));
    EXPECT_TRUE(rejects("subtype s is t(0 to 3) range 0 to 1;"));
    EXPECT_TRUE(rejects("subtype s is ieee.numeric_std.unsigned;")) << "selected type marks are not supported";
}
