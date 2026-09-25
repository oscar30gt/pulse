#ifndef PULSE_COMPILER_SEMANTIC_TYPE_H
#define PULSE_COMPILER_SEMANTIC_TYPE_H

// Resolved (semantic) types used by the analyzer.
//
// The AST only holds *names* of types (TypeSpec). The analyzer resolves each name once into a
// SemanticType, and from then on every question ("are these compatible?", "how wide is this?")
// is answered by comparing TypeInfo identity, never strings. A subtype shares the TypeInfo of its
// base type and only carries its own constraint, so `natural` and `integer` are the same type
// with different ranges, while two same-named types from different scopes stay different.

#include "ast.h"

#include <optional>
#include <string>
#include <vector>

namespace Pulse::Parser
{
    enum class TypeClass { Enumeration, Integer, Real, Physical, Array, Record };

    /// Which IEEE vector type an array is, for the operators numeric_std/std_logic_1164 define.
    enum class VectorFamily { None, StdLogicVector, Unsigned, Signed };

    struct TypeInfo;

    /// Static bounds of one array dimension. Direction matters for slices; length does not.
    struct Bounds
    {
        int64_t left = 0;
        int64_t right = 0;
        bool ascending = false;     /// `to` (true) or `downto` (false)

        int64_t low() const { return ascending ? left : right; }
        int64_t high() const { return ascending ? right : left; }
        int64_t length() const { return high() < low() ? 0 : high() - low() + 1; }
    };

    /// Range of a scalar subtype. Integers, enumeration positions and physical base units use
    /// `low/high`; real types use `realLow/realHigh`.
    struct ScalarRange
    {
        bool present = false;
        int64_t low = 0;
        int64_t high = 0;
        double realLow = 0.0;
        double realHigh = 0.0;
    };

    /// A type together with its constraint: what an expression, object or subtype indication *is*.
    struct SemanticType
    {
        const TypeInfo* info = nullptr;
        ScalarRange range;              /// Scalar constraint (empty for arrays/records)
        std::vector<Bounds> dims;       /// Array constraint, one entry per dimension; empty = unconstrained or unknown
        /// Array constrained by bounds that exist but are not known at analysis time (they depend on generics):
        /// the type counts as constrained, and every length check involving it is skipped.
        bool unknownBounds = false;
        bool resolved = false;          /// The subtype carries a resolution function (several drivers are legal)

        bool valid() const { return info != nullptr; }
    };

    /// The identity of a type: created once per `type` declaration (and once per predefined universal type).
    struct TypeInfo
    {
        struct Unit { std::string name; int64_t factor = 1; };     /// factor = multiples of the base unit
        struct Field { std::string name; SemanticType type; };

        std::string name;
        TypeClass cls = TypeClass::Integer;
        bool isResolved = false;                    /// Multiple drivers allowed (std_logic and arrays of it)
        VectorFamily family = VectorFamily::None;

        ScalarRange range;                          /// Declared range of a scalar type
        std::vector<std::string> literals;          /// Enumeration literals in declared order
        std::vector<Unit> units;                    /// Physical units; the first is the base unit

        SemanticType element;                       /// Array element type
        std::vector<SemanticType> indexTypes;       /// Array index type per dimension
        std::vector<Bounds> declaredDims;           /// Constraint of `array (0 to 3) of ...`; empty when unconstrained
        bool declaredUnknownBounds = false;         /// Constrained by bounds that depend on generics
        std::vector<Field> fields;                  /// Record fields in declared order
    };

    // ---- Universal types ------------------------------------------------------------------------

    /// Types of integer and real literals; implicitly convertible to any integer/real type.
    const TypeInfo& universalIntegerInfo();
    const TypeInfo& universalRealInfo();
    SemanticType universalInteger();
    SemanticType universalReal();

    // ---- Queries --------------------------------------------------------------------------------

    bool isUniversal(const SemanticType& type);
    bool isUniversalInteger(const SemanticType& type);
    bool isUniversalReal(const SemanticType& type);
    bool isIntegerClass(const SemanticType& type);      /// Integer types, including universal integer
    bool isRealClass(const SemanticType& type);
    bool isNumeric(const SemanticType& type);           /// Integer, real or physical
    bool isDiscrete(const SemanticType& type);          /// Integer or enumeration
    bool isScalar(const SemanticType& type);
    bool isArray(const SemanticType& type);
    bool isRecord(const SemanticType& type);
    bool isOneDimensionalArray(const SemanticType& type);

    /// The unconstrained-by-usage view of a type: its base type with the constraint it was declared with.
    SemanticType typeOf(const TypeInfo& info);

    /// The same type without any constraint (used where lengths must not be compared).
    SemanticType withoutConstraint(const SemanticType& type);

    /// Number of elements of a fully constrained array (all dimensions), if known.
    std::optional<int64_t> staticLength(const SemanticType& type);

    /// An array that has an index constraint, whether or not its bounds are known.
    bool isConstrainedArray(const SemanticType& type);

    /// Human-readable form used in diagnostics, e.g. `std_logic_vector(7 downto 0)`.
    std::string describe(const SemanticType& type);

    /// Text of a bounds pair, e.g. `7 downto 0`.
    std::string describeBounds(const Bounds& bounds);

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_SEMANTIC_TYPE_H
