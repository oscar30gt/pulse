#ifndef PULSE_COMPILER_OPERATOR_RULES_H
#define PULSE_COMPILER_OPERATOR_RULES_H

// Predefined operator rules of VHDL, as pure functions over already-resolved operand types.
//
// A type gets exactly the operators the LRM predefines for it, plus the numeric_std / std_logic_1164 operators for
// the IEEE vectors. Operators a design declares itself (`function "+"(a, b : t) return t`) are resolved by the
// analyzer before these rules are consulted.

#include "semantic_type.h"

#include <string>

namespace Pulse::Parser
{
    /// Outcome of applying an operator: the result type, or a `problem` sentence for the diagnostic.
    struct RuleResult
    {
        SemanticType type;
        std::string problem;

        bool ok() const { return problem.empty(); }
        static RuleResult success(SemanticType type) { return { std::move(type), {} }; }
        static RuleResult failure(std::string problem) { return { {}, std::move(problem) }; }
    };

    /// The predefined operators of VHDL as pure functions over already-resolved operand types.
    ///
    /// Every type gets exactly the operators the LRM predefines for it, plus the numeric_std / std_logic_1164
    /// operators for the IEEE vectors (unsigned, signed, std_logic_vector), whose result widths follow numeric_std
    /// (`+`/`-` give the wider operand, `*` the sum of the widths, ...). Literal operands must be typed by the caller
    /// before the rules run. Implemented in operator_rules.cc (numeric, relational) and
    /// operator_arrays.cc (logical, shifts, concatenation, numeric_std, matching).
    class OperatorRules
    {
        const TypeInfo* m_boolean = nullptr;
        const TypeInfo* m_stdLogic = nullptr;

    public:
        /// Rules without predefined types; only usable for operators that do not produce booleans (the prelude uses these).
        OperatorRules() = default;
        /// Rules that produce `boolean` and `std_logic` (TypeInfos of the prelude) for comparisons.
        OperatorRules(const TypeInfo* boolean, const TypeInfo* stdLogic) : m_boolean(boolean), m_stdLogic(stdLogic) { }

        /// `expected` is the type demanded by the context; only `&` between two elements needs it.
        RuleResult binary(BinaryOperator op, const SemanticType& left, const SemanticType& right,
                          const SemanticType* expected) const;

        /// Result type of a unary operator: sign, `abs`, `not`, `??` and the logical reductions.
        RuleResult unary(UnaryOperator op, const SemanticType& operand) const;

    private:
        // -- numeric (operator_rules.cc)
        /// `+` and `-`: numbers of the same type, or physical quantities of the same type.
        RuleResult additive(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;
        /// `*`, `/`, `mod`, `rem`: numbers of the same type; a physical value times or divided by a number; two physical
        /// values of one type divided give a universal integer.
        RuleResult multiplicative(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;
        /// `**`: integer or real base with an integer exponent; the result has the type of the base.
        RuleResult exponent(const SemanticType& l, const SemanticType& r) const;
        /// Unary `+`, `-`, `abs` on numbers; `-` and `abs` also on `signed` vectors.
        RuleResult signedUnary(UnaryOperator op, const SemanticType& operand) const;

        // -- relational (operator_rules.cc)
        /// `=` and `/=`: any two operands of the same type (universal literals unify with typed numbers).
        RuleResult equality(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;
        /// `<`, `<=`, `>`, `>=`: scalars, and one-dimensional arrays of scalars, of the same type.
        RuleResult ordering(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;

        // -- arrays: logical, shift, concatenation, numeric_std (operator_arrays.cc)
        /// `and or nand nor xor xnor` on boolean, std_logic and arrays of them; both operands need the same type and, for
        /// arrays, the same length when it is known.
        RuleResult logical(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;
        /// `not` on the same operand types as the binary logical operators.
        RuleResult logicalNot(const SemanticType& operand) const;
        /// `and v`, `or v`, ...: a logical reduction of a one-dimensional array of boolean or std_logic to its element type.
        RuleResult reduction(UnaryOperator op, const SemanticType& operand) const;
        /// `sll srl sla sra rol ror`: the left operand is a vector, the right an integer; the result has the left type.
        RuleResult shift(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;
        /// `&` between arrays of one type, an array and an element, or (given `expected`) two elements. The result length is
        /// the sum of the lengths when both are known.
        RuleResult concatenate(const SemanticType& l, const SemanticType& r, const SemanticType* expected) const;
        /// Arithmetic on `unsigned`/`signed` with numeric_std widths: vector op vector (same type) or vector op integer.
        RuleResult numericVector(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;
        /// Comparisons of `unsigned`/`signed` with the same vector type or with an integer.
        RuleResult vectorComparison(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;

        // -- VHDL-2008 matching (operator_arrays.cc)
        /// `?= ?/= ?< ?<= ?> ?>=`: std_logic values, or arrays of them (`?=`, `?/=`), or unsigned/signed vectors; the result
        /// is std_logic.
        RuleResult matching(BinaryOperator op, const SemanticType& l, const SemanticType& r) const;
        /// `?? x`: a std_logic (or boolean) value converted to boolean.
        RuleResult condition(const SemanticType& operand) const;
    };

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_OPERATOR_RULES_H
