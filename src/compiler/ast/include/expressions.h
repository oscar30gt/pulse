#ifndef PULSE_VHDL_AST_EXPRESSIONS_H
#define PULSE_VHDL_AST_EXPRESSIONS_H

#include "node.h"
#include "operators.h"

namespace Pulse::Parser
{
    // --------------------------------------------------------------------------------------------
    // Names
    // --------------------------------------------------------------------------------------------

    /// A plain identifier: a signal/port/constant/variable, or (when no such object exists) an
    /// enumeration literal or physical unit. The parser cannot tell which, the analyzer resolves it.
    struct SymbolExpr final : Expression
    {
        std::string name;   /// Identifier as written (lowercased)

        PULSE_AST_NODE_OVERRIDES
    };

    /// Record field access: "<target>.<fieldName>", e.g. "p.x". The target may itself be a name (p.a.b).
    struct FieldAccessExpr final : Expression
    {
        ExpressionPtr target;
        std::string fieldName;

        ~FieldAccessExpr() override { unlinkSpine(target); }
        ExpressionPtr* spineChild() override { return &target; }

        PULSE_AST_NODE_OVERRIDES
    };

    /// Any `prefix(arguments)` form: an indexed name, a slice, a function call or a type conversion.
    /// They are syntactically identical in VHDL; the analyzer decides which one it is.
    struct FunctionCallExpr final : Expression
    {
        ExpressionPtr callee;                   /// The prefix being applied (usually a SymbolExpr)
        std::vector<ExpressionPtr> arguments;   /// Arguments; a range (a `To`/`Downto` BinaryOpExpr) denotes a slice

        ~FunctionCallExpr() override { unlinkSpine(callee); }
        ExpressionPtr* spineChild() override { return &callee; }

        PULSE_AST_NODE_OVERRIDES
    };

    /// Signature between a prefix and its attribute tick: `name[t1, t2 return t3]'attr`.
    struct SignatureExpr final : Expression
    {
        std::vector<ExpressionPtr> parameters;  /// Parameter type marks
        ExpressionPtr returnType;               /// Return type mark, or null when there is no `return`

        PULSE_AST_NODE_OVERRIDES
    };

    /// Attribute of a name, such as "signal_name'left", "clk'event" or "arr'range".
    struct AttributeExpr final : Expression
    {
        ExpressionPtr prefix;                   /// The name the attribute is applied to
        std::unique_ptr<SignatureExpr> signature; /// Optional signature of the prefix, or null
        std::string attributeName;              /// Attribute designator as written (lowercased)

        ~AttributeExpr() override { unlinkSpine(prefix); }
        ExpressionPtr* spineChild() override { return &prefix; }

        PULSE_AST_NODE_OVERRIDES
    };

    /// Qualified expression `type_mark'(operand)`; the operand is a parenthesized expression or an aggregate.
    struct QualifiedExpr final : Expression
    {
        ExpressionPtr typeMark;
        ExpressionPtr operand;

        PULSE_AST_NODE_OVERRIDES
    };

    /// `formal => actual`: a named association in a call, an index constraint, a port map or an aggregate.
    struct NamedAssociationExpr final : Expression
    {
        ExpressionPtr formal;   /// The formal part: a name (possibly a conversion call), or a choice list in an aggregate
        ExpressionPtr actual;   /// The actual part; an OpenExpr for `open`

        PULSE_AST_NODE_OVERRIDES
    };

    /// The reserved word `open` used as an actual or as an unconstrained index constraint.
    struct OpenExpr final : Expression
    {
        PULSE_AST_NODE_OVERRIDES
    };

    /// Kind of object an external name refers to.
    enum class ExternalObjectClass : uint8_t { Constant, Signal, Variable };

    struct TypeSpec;

    /// External name (VHDL-2008): `<<signal .tb.dut.count : unsigned(3 downto 0)>>`.
    struct ExternalNameExpr final : Expression
    {
        ExternalObjectClass objectClass = ExternalObjectClass::Signal;
        std::string path;                       /// The path exactly as written, tokens joined (`.tb.dut.count`, `@lib.pkg.obj`, `^.^.x`)
        std::unique_ptr<TypeSpec> subtype;      /// The subtype after the colon

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Subtype indications
    // --------------------------------------------------------------------------------------------

    /// A type mark with its optional constraints: "std_logic", "unsigned(7 downto 0)", "integer range 0 to 9".
    /// It is an expression so that a bare type mark can also stand wherever a name is allowed (a qualified
    /// expression, a signature, an attribute prefix ...).
    struct TypeSpec final : Expression
    {
        ExpressionPtr resolution;               /// Resolution indication in front of the type mark ("resolved", "(resolved)"), or null
        std::string typeName;                   /// Name of the type (e.g. "std_logic", "integer")
        std::vector<ExpressionPtr> args;        /// Index constraints between parentheses; ranges or OpenExpr
        ExpressionPtr range;                    /// The constraint after `range` (a range or a name), or null

        PULSE_AST_NODE_OVERRIDES
    };

    /// Element resolution of an array subtype: the parenthesized `(resolved)` in "(resolved) std_logic_vector".
    struct ElementResolutionExpr final : Expression
    {
        ExpressionPtr resolution;   /// The resolution of each element: a name, or another ElementResolutionExpr

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Literals
    // --------------------------------------------------------------------------------------------

    /// Plain integer constant.
    struct IntegerLiteralExpr final : Expression
    {
        int64_t value = 0;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Plain floating-point constant, e.g. "3.14" or "1.0e-6".
    struct DoubleLiteralExpr final : Expression
    {
        double value = 0.0;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Physical literal such as "10 ns" or "1.5 us": a numeric magnitude followed by a unit name.
    /// The parser performs no lookup; the analyzer resolves the unit and the owning physical type.
    struct PhysicalLiteralExpr final : Expression
    {
        ExpressionPtr magnitude;   /// IntegerLiteralExpr or DoubleLiteralExpr
        std::string unit;          /// Unit name as written (lowercased)

        PULSE_AST_NODE_OVERRIDES
    };

    /// A character literal such as '0' or 'X'. Resolved as an enumeration literal.
    struct CharacterLiteralExpr final : Expression
    {
        std::string value;   /// The literal including its quotes (e.g. "'X'"), matching enumeration literal text

        PULSE_AST_NODE_OVERRIDES
    };

    /// A string literal: "hello", "0101", x"FF", 8sx"F". Every string is a string of characters; a bit-string
    /// prefix only expands its digits into characters (x"F" is "1111"), so logic vectors are strings too.
    struct StringLiteralExpr final : Expression
    {
        std::string value;   /// The characters, with doubled quotes ("") un-escaped and bit strings expanded

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Choices and aggregates
    // --------------------------------------------------------------------------------------------

    /// The choice `others`; only meaningful as the sole alternative of a ChoiceListExpr.
    struct OthersExpr final : Expression
    {
        PULSE_AST_NODE_OVERRIDES
    };

    /// The reserved word `all` as an entity name list (attribute specifications).
    struct AllExpr final : Expression
    {
        PULSE_AST_NODE_OVERRIDES
    };

    /// One `|`-separated choice list, as used by `case`, `with ... select` and named aggregate associations:
    /// "a | b | 0 to 3". Each alternative is an expression or a range; `others` is a list holding one OthersExpr.
    struct ChoiceListExpr final : Expression
    {
        std::vector<ExpressionPtr> alternatives;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Aggregate: "(a, b, c)", "(0 => a, 1 => b)" or "(others => '0')".
    struct AggregateExpr final : Expression
    {
        /// Each element is either a positional expression or a NamedAssociationExpr whose formal is a ChoiceListExpr.
        std::vector<ExpressionPtr> elements;

        PULSE_AST_NODE_OVERRIDES
    };

    // --------------------------------------------------------------------------------------------
    // Operators
    // --------------------------------------------------------------------------------------------

    /// Binary expression: "a + b", "a and b", "a sll 2" ... and, where a range is allowed, "hi downto lo".
    struct BinaryOpExpr final : Expression
    {
        BinaryOperator op = BinaryOperator::Add;
        ExpressionPtr left;
        ExpressionPtr right;

        ~BinaryOpExpr() override { unlinkSpine(left); }
        ExpressionPtr* spineChild() override { return &left; }

        PULSE_AST_NODE_OVERRIDES
    };

    /// Unary expression: "-a", "+a", "not a", "abs a", "and v".
    struct UnaryOpExpr final : Expression
    {
        UnaryOperator op = UnaryOperator::Plus;
        ExpressionPtr operand;

        PULSE_AST_NODE_OVERRIDES
    };

    /// Conditional value, a ternary: "trueValue when condition else falseValue".
    /// More branches are further WhenElseExprs in `falseValue`:
    /// "a when c1 else b when c2 else d" is WhenElse(a, c1, WhenElse(b, c2, d)).
    struct WhenElseExpr final : Expression
    {
        ExpressionPtr trueValue;    /// Value when the condition holds
        ExpressionPtr condition;    /// Boolean expression
        ExpressionPtr falseValue;   /// Value otherwise: any expression (another WhenElseExpr for `elsif`-like chains), or null when the final `else` is omitted

        ~WhenElseExpr() override { unlinkSpine(falseValue); }
        ExpressionPtr* spineChild() override { return &falseValue; }

        PULSE_AST_NODE_OVERRIDES
    };

    /// The reserved word `unaffected` used as a value (VHDL-2008 conditional/selected assignments).
    struct UnaffectedExpr final : Expression
    {
        PULSE_AST_NODE_OVERRIDES
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_EXPRESSIONS_H
