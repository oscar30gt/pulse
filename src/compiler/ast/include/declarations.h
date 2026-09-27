#ifndef PULSE_VHDL_AST_DECLARATIONS_H
#define PULSE_VHDL_AST_DECLARATIONS_H

#include "node.h"
#include "expressions.h"

namespace Pulse::Parser
{
    // --------------------------------------------------------------------------------------------
    // Type definitions (right-hand side of `type <name> is <definition>;`)
    // --------------------------------------------------------------------------------------------

    /// `type <name> is range <expr>;` — an integer- or floating-point-range type.
    /// Whether the type is integer- or real-based is determined by the literal kind of the range's bounds.
    struct NumericTypeDefinition final : TypeDefinition
    {
        ExpressionPtr range;    /// A BinaryOpExpr with op To/Downto, e.g. "0 to 255"

        PULSE_AST_NODE_OVERRIDES
    };

    /// `type <name> is (A, B, C);` or `type <name> is ('0', '1', 'Z', 'X');`
    struct EnumeratedTypeDefinition final : TypeDefinition
    {
        /// Enumeration literals in declared order: plain identifiers (e.g. "a") or character
        /// literals stored *with* their quotes (e.g. "'0'"), so the two can never collide.
        std::vector<std::string> literals;

        PULSE_AST_NODE_OVERRIDES
    };

    /// A single unit inside the `units ... end units` block.
    struct UnitDeclaration final : ASTNode
    {
        std::string name;           /// Name of this unit (e.g. "ps")
        ExpressionPtr multiplier;   /// Multiplier literal (e.g. 1000), null for the base unit
        std::string ofUnit;         /// Name of the unit this one is defined in terms of, empty for the base unit

        PULSE_AST_NODE_OVERRIDES
    };

    /// `type <name> is range <expr> units <base>; <unit> = <n> <ofUnit>; ... end units;`
    struct PhysicalTypeDefinition final : TypeDefinition
    {
        ExpressionPtr range;                                /// Base range for the underlying integer representation
        std::vector<std::unique_ptr<UnitDeclaration>> units;/// Unit declarations, in declared order (first entry is the base unit)

        PULSE_AST_NODE_OVERRIDES
    };

    /// `type <name> is array (<range>[, <range>...]) of <elementType>;`
    struct ArrayTypeDefinition final : TypeDefinition
    {
        std::vector<ExpressionPtr> indexRanges;     /// One range (a To/Downto BinaryOpExpr) per dimension
        std::unique_ptr<TypeSpec> elementType;      /// Type of each array element

        PULSE_AST_NODE_OVERRIDES
    };

    /// `type <name> is array (<indexType> range <>[, ...]) of <elementType>;`
    struct UnconstrainedArrayTypeDefinition final : TypeDefinition
    {
        std::vector<std::string> indexTypeMarks;    /// Index type mark per dimension (e.g. "natural")
        std::unique_ptr<TypeSpec> elementType;      /// Type of each array element

        PULSE_AST_NODE_OVERRIDES
    };

    /// A single field of a record.
    struct RecordField final : ASTNode
    {
        std::string name;
        std::unique_ptr<TypeSpec> type;

        PULSE_AST_NODE_OVERRIDES
    };

    /// `type <name> is record <field>: <type>; ... end record;`
    struct RecordTypeDefinition final : TypeDefinition
    {
        std::vector<std::unique_ptr<RecordField>> fields;   /// Fields, in declared order

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Declarations (declarative parts of architectures, processes and components)
    // --------------------------------------------------------------------------------------------

    /// Declaration of a user-defined type: `type <name> is <definition>;`
    struct TypeDeclaration final : Declaration
    {
        std::string name;
        TypeDefinitionPtr definition;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Declaration of a subtype: `subtype <name> is <baseType>;`
    struct SubtypeDeclaration final : Declaration
    {
        std::string name;
        std::unique_ptr<TypeSpec> baseType;     /// e.g. "std_logic_vector(7 downto 0)", "integer range 0 to 9"

        PULSE_AST_NODE_OVERRIDES
    };

    /// Declaration of a signal: `signal <name> : <type> [:= <value>];` (one node per declared name).
    struct SignalDeclaration final : Declaration
    {
        std::string name;
        std::unique_ptr<TypeSpec> typeSpec;
        ExpressionPtr initialValue;             /// Optional initial value, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// Declaration of a constant: `constant <name> : <type> := <value>;` (one node per declared name).
    struct ConstantDeclaration final : Declaration
    {
        std::string name;
        std::unique_ptr<TypeSpec> typeSpec;
        ExpressionPtr value;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Declaration of a variable (processes only): `variable <name> : <type> [:= <value>];`.
    struct VariableDeclaration final : Declaration
    {
        std::string name;
        std::unique_ptr<TypeSpec> typeSpec;
        ExpressionPtr initialValue;             /// Optional initial value, or null

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Interface declarations (generics, ports and subprogram parameters)
    // --------------------------------------------------------------------------------------------

    /// Direction of a port or a subprogram parameter.
    enum class PortMode : uint8_t { In, Out, InOut };

    /// Declaration of a generic constant: `N : natural := 8` (one node per declared name).
    struct GenericDeclaration final : Declaration
    {
        std::string name;
        std::unique_ptr<TypeSpec> typeSpec;
        ExpressionPtr defaultValue;             /// Value after `:=`, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// Declaration of a port (one node per declared name; the mode defaults to `in`).
    struct PortDeclaration final : Declaration
    {
        std::string name;
        std::unique_ptr<TypeSpec> typeSpec;
        PortMode mode = PortMode::In;
        ExpressionPtr defaultValue;             /// Value after `:=`, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// Object class written in front of a subprogram parameter.
    enum class ParameterClass : uint8_t { Unspecified, Constant, Signal, Variable };

    /// A subprogram parameter: `[class] name : [mode] type [:= default]` (one node per declared name).
    struct ParameterDeclaration final : Declaration
    {
        ParameterClass objectClass = ParameterClass::Unspecified;
        std::string name;
        PortMode mode = PortMode::In;
        std::unique_ptr<TypeSpec> typeSpec;
        ExpressionPtr defaultValue;             /// Value after `:=`, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// Declaration of a component: `component <name> [generic (...);] [port (...);] end component;`.
    struct ComponentDeclaration final : Declaration
    {
        std::string name;
        std::vector<std::unique_ptr<GenericDeclaration>> generics;
        std::vector<std::unique_ptr<PortDeclaration>> ports;

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Context clauses
    // --------------------------------------------------------------------------------------------

    /// `library ieee, work;`
    struct LibraryClause final : Declaration
    {
        std::vector<std::string> names;

        PULSE_AST_NODE_OVERRIDES
    };

    /// `use ieee.std_logic_1164.all, work.pkg.x;` Each name is a selected name (FieldAccessExpr chain, `.all`
    /// being a field named "all").
    struct UseClause final : Declaration
    {
        std::vector<ExpressionPtr> names;

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Subprograms
    // --------------------------------------------------------------------------------------------

    enum class SubprogramKind : uint8_t { Function, Procedure };

    /// The header of a subprogram: `[pure|impure] function name [(parameters)] return type` or
    /// `procedure name [(parameters)]`.
    struct SubprogramSpec final : ASTNode
    {
        SubprogramKind kind = SubprogramKind::Procedure;
        bool impure = false;                                        /// `impure function`
        std::string name;                                           /// Identifier, or an operator symbol with its quotes ("\"and\"")
        std::vector<std::unique_ptr<ParameterDeclaration>> parameters;
        std::unique_ptr<TypeSpec> returnType;                       /// Return type mark of a function, null for a procedure

        PULSE_AST_NODE_OVERRIDES
    };

    /// A subprogram declaration without a body: `function f(a : bit) return bit;`
    struct SubprogramDeclaration final : Declaration
    {
        std::unique_ptr<SubprogramSpec> spec;

        PULSE_AST_NODE_OVERRIDES
    };

    /// A subprogram body: `<spec> is <declarations> begin <statements> end [kind] [name];`
    struct SubprogramBody final : Declaration
    {
        std::unique_ptr<SubprogramSpec> spec;
        std::vector<DeclarationPtr> declarations;   /// Declarative part, in source order
        std::vector<StatementPtr> body;             /// Sequential statements

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Aliases and attributes
    // --------------------------------------------------------------------------------------------

    /// `alias name [: subtype] is target [signature];`
    struct AliasDeclaration final : Declaration
    {
        std::string name;                           /// Identifier, character literal or operator symbol, as written
        std::unique_ptr<TypeSpec> subtype;          /// Subtype after the colon, or null
        ExpressionPtr target;                       /// The aliased name
        std::unique_ptr<SignatureExpr> signature;   /// Signature of an aliased subprogram, or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// `attribute name : type_mark;`
    struct AttributeDeclaration final : Declaration
    {
        std::string name;
        std::unique_ptr<TypeSpec> typeMark;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Class of the items an attribute specification applies to (`: signal`, `: entity` ...).
    enum class EntityClass : uint8_t
    {
        Entity, Architecture, Configuration, Procedure, Function, Package, Type, Subtype, Constant, Signal,
        Variable, Component, Label, Literal, Units, Group, File, Property, Sequence,
    };

    /// `attribute name of entities : class is value;`
    struct AttributeSpecification final : Declaration
    {
        std::string attributeName;
        std::vector<ExpressionPtr> entities;        /// SymbolExprs, or a single OthersExpr / AllExpr
        EntityClass entityClass = EntityClass::Signal;
        ExpressionPtr value;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Source spelling of a port mode and an entity class.
    const char* toString(PortMode mode);
    const char* toString(EntityClass entityClass);

    // --------------------------------------------------------------------------------------------
    // Design units
    // --------------------------------------------------------------------------------------------

    /// Declaration of an entity.
    struct EntityDeclaration final : DesignUnit
    {
        std::string name;
        std::vector<std::unique_ptr<GenericDeclaration>> generics;
        std::vector<std::unique_ptr<PortDeclaration>> ports;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Declaration of an architecture.
    struct ArchitectureDeclaration final : DesignUnit
    {
        std::string entityName;                     /// Entity this architecture belongs to
        std::string name;                           /// Name of this architecture
        std::vector<DeclarationPtr> declarations;   /// Declarative part (between `is` and `begin`), in source order
        std::vector<StatementPtr> body;             /// Concurrent statements (between `begin` and `end`), in source order

        PULSE_AST_NODE_OVERRIDES
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_DECLARATIONS_H
