#ifndef PULSE_COMPILER_ANALYZER_SCOPE_H
#define PULSE_COMPILER_ANALYZER_SCOPE_H

#include "semantic_type.h"

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Pulse::Parser
{
    /// What a name in a scope stands for. Signals include ports; enumeration literals and physical units are not
    /// symbols: they belong to their type and are found through it.
    enum class SymbolKind
    {
        Signal,         ///< Signal or port (ports carry a PortMode)
        Constant,       ///< Constant or generic, possibly with a statically known value
        Variable,       ///< Process variable
        LoopParameter,  ///< Parameter of a `for` loop; read-only and local to the loop
        Type,           ///< `type` or `subtype` name; `type` holds the subtype indication it stands for
        Component,      ///< Component declaration
        Label,          ///< Label of a statement; not a value
        Subprogram,     ///< Function or procedure name (possibly several overloads)
        Attribute,      ///< Attribute declared with `attribute name : type;`
    };

    /// A value known at analysis time: an integer, an enumeration position, a physical quantity
    /// in base units, or a real number. `type` is the (possibly universal) type it belongs to.
    struct ConstValue
    {
        const TypeInfo* type = nullptr;
        int64_t integer = 0;     /// Integer value, enumeration position or physical value in base units
        double real = 0.0;       /// Real value

        bool isReal() const { return type && type->cls == TypeClass::Real; }
        double asReal() const { return isReal() ? real : static_cast<double>(integer); }
    };

    /// One formal of a component, entity or subprogram: what a map or a call associates an actual with.
    struct FormalInfo
    {
        std::string name;
        SymbolKind kind = SymbolKind::Constant;     ///< Signal, Constant or Variable
        PortMode mode = PortMode::In;
        SemanticType type;
        bool hasDefault = false;
        SourceLocation location;
    };

    struct SubprogramInfo;

    /// The value an `attribute x of item : class is value;` specification gives an item.
    struct AttributeValue
    {
        SemanticType type;
        std::optional<ConstValue> value;    ///< Folded value, when it is known at analysis time
        SourceLocation location;
    };

    /// One entry of a scope: an object, a type/subtype, a component or a subprogram.
    struct Symbol
    {
        SymbolKind kind = SymbolKind::Signal;
        SemanticType type;                      /// Object type, or the subtype indication a type name stands for
        std::optional<PortMode> mode;           /// Ports and parameters only
        std::optional<ConstValue> value;        /// Constants with a statically known value
        bool declaresBaseType = false;          /// Type entries created by `type` (not `subtype`): owners of literals/units
        bool isGeneric = false;                 /// A generic constant: its value is only known per instance
        bool isPort = false;                    /// A port of the entity of the architecture being analyzed
        bool isParameter = false;               /// A formal parameter of a subprogram
        bool partialAlias = false;              /// An alias of a part of an object (an element, a slice or a field)
        size_t objectId = 0;                    /// Identity of the object: an alias shares the id of its target
        size_t depth = std::numeric_limits<size_t>::max();   ///< Scope index of the declaration (set by declare(); aliases keep their target's)
        const ComponentDeclaration* component = nullptr;
        std::vector<FormalInfo> componentGenerics;  ///< Components: generics in declaration order
        std::vector<FormalInfo> componentPorts;     ///< Components: ports in declaration order
        std::vector<const SubprogramInfo*> overloads;   ///< Subprograms: every overload declared under this name in this region
        std::unordered_map<std::string, AttributeValue> attributes; ///< Attributes specified for this item, by attribute name
        SourceLocation location;
    };

    /// A declarative region. Names must be unique inside one scope (subprograms may share a name); inner scopes may
    /// shadow outer ones.
    struct Scope
    {
        std::unordered_map<std::string, Symbol> symbols;
        /// Names of the types (not subtypes) declared here, in declaration order: they own enumeration literals and physical units.
        /// Kept apart so that looking a literal up visits the few types of a region, not every name in it.
        std::vector<std::string> baseTypes;
        /// Every enumeration literal and physical unit of those types, to reject a clashing declaration in constant time.
        std::unordered_set<std::string> literalsAndUnits;
        /// Attributes of the things that are not symbols (enumeration literals, physical units), by name then attribute.
        std::unordered_map<std::string, std::unordered_map<std::string, AttributeValue>> itemAttributes;
    };

    /// A physical unit found by name, with the type that owns it.
    struct UnitRef
    {
        const TypeInfo* type = nullptr;
        int64_t factor = 1;
    };

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_ANALYZER_SCOPE_H
