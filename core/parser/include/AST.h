#ifndef PULSE_VHDL_ASTBUILDER_H
#define PULSE_VHDL_ASTBUILDER_H

#include "Tokenizer.h"
#include "signalInterface.h"
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <unordered_map>

namespace Pulse::Parser::VHDL
{
    /// Return type of an expression
    enum class ReturnType
    {
        LOGIC,
        INTEGER,
        SIGNED,
        UNSIGNED,
        BOOLEAN,
    };

    // --------------------------------------------------------------------------------------------

    /// Base class for all AST Nodes to allow proper polymorphism without slicing.
    struct ASTNode { virtual ~ASTNode() = default; };

    /// Root node containing top-level entities and architectures
    struct ASTRoot final : ASTNode
    {
        /// Top-level entities and architectures in the order they were parsed.
        std::vector<std::unique_ptr<ASTNode>> children;

        /// Print the AST tree to stdout in a human-readable format.
        /// This is primarily for debugging and visualization purposes.
        void print() const;
    };

    // --------------------------------------------------------------------------------------------

    /// Flattened port declaration. When parsing a port from vhdl, formats can be very different.
    /// Here, the declaration is normalized to a single name, width, and direction.
    /// For ports declared as STD_LOGIC, the width is 1. For STD_LOGIC_VECTOR, 
    /// the width is the number of bits in the vector.
    /// For STD_LOGIC_VECTOR declared with a range different to (N downto 0), 
    /// the width is still the number of bits in the vector and signal accesses will be normalized 
    /// to the flattened bit positions.
    struct PortDeclaration : ASTNode
    {
        std::string portName;       ///< Name of the port
        Pulse::bitWidth_t width;    ///< Width of the port in bits
        bool isInput;               ///< True if the port is an input, false if it's an output
    };

    /// Declaration of an entity in VHDL.
    /// Entities support multiple ports.
    /// [TODO]: For now, generic parameters are not supported.
    struct EntityDeclaration : ASTNode
    {
        std::string entityName;     ///< Name of the entity
        std::vector<std::unique_ptr<PortDeclaration>> ports;    ///< Ports of the entity
    };

    struct SignalDeclaration;
    struct ComponentDeclaration;
    struct SignalAssignment;
    struct ComponentInstantiation;
    struct ProcessStatement;

    /// Declaration of an architecture in VHDL.
    /// Architectures support multiple signals, components, assignments,
    /// instantiations, and processes.
    struct ArchitectureDeclaration : ASTNode
    {
        std::string architectureName; ///< Name of the architecture
        std::string entityName;       ///< Name of the entity

        std::vector<std::unique_ptr<SignalDeclaration>> signals;            ///< Signals declared in the architecture
        std::vector<std::unique_ptr<ComponentDeclaration>> components;      ///< Components declared in the architecture
        std::vector<std::unique_ptr<SignalAssignment>> assignments;         ///< Signal assignments in the order they were parsed
        std::vector<std::unique_ptr<ComponentInstantiation>> instantiations;///< Component instantiations in the order they were parsed
        std::vector<std::unique_ptr<ProcessStatement>> processes;           ///< Process statements in the order they were parsed
    };

    /// Declaration of a signal in VHDL.
    /// Range flattenning is applied to the signal width the same way as for ports.
    struct SignalDeclaration : ASTNode
    {
        std::string signalName;     ///< Name of the signal
        Pulse::bitWidth_t width;    ///< Width of the signal in bits
        uint64_t initialValue;      ///< Initial value of the signal, if any. Not implemented yet.
    };

    /// Declaration of a component in VHDL.
    struct ComponentDeclaration : ASTNode
    {
        std::string componentName;  ///< Name of the component
        std::vector<std::unique_ptr<PortDeclaration>> ports;  ///< Ports of the component
    };

    /// A reference to a signal or a subset of its bits.
    /// Can be used as a target for assignments or as a source in expressions.
    struct SignalReference;

    // Component instantiation in architecture
    struct ComponentInstantiation : ASTNode
    {
        std::string instanceName;   ///< Name of the component instance
        std::string componentName;  ///< Name of the component being instantiated
        std::unordered_map<std::string, std::unique_ptr<SignalReference>> portMaps; ///< Mapping of component ports to signals in the architecture
    };

    ///////////////////////////////////////////////////////////////////////////

    /// Base class for all evaluable expressions.
    /// @tparam R The return type of the expression
    template <ReturnType R>
    struct Expression : ASTNode
    {
        ReturnType returnType = R;  ///< Type this expression evaluates to.
    };

    /// Base class for sequential statements that can appear inside a process body.
    /// Concrete types: SignalAssignment, IfStatement, WaitForStatement.
    struct SequentialStatement : ASTNode {};

    /// signal_name(4 downto 0) <= x"00AA"
    struct SignalAssignment final : SequentialStatement
    {
        std::unique_ptr<SignalReference> target;    ///< Target signal reference for the assignment
        std::unique_ptr<Expression<ReturnType::LOGIC>> value;   ///< Value expression to be assigned to the target
    };

    /// signal_name ; signal_name(4 downto 0) ; signal_name(3) ; signal_name(7 to 4)...
    /// low/high are always stored FLATTENED (bit 0 = the internal LSB position, see
    /// ASTBuilder.cc for the normalization rules). Both null => full signal reference.
    /// Only 'low' set => single-bit index. Both set => range (high/low may come out
    /// "inverted", i.e. high < low, if the access direction differs from how the
    /// signal was declared).
    struct SignalReference final : Expression<ReturnType::LOGIC>
    {
        std::string signalName; ///< Name of the signal being referenced
        std::unique_ptr<Expression<ReturnType::INTEGER>> low;  ///< Less significant bit index, null if full signal
        std::unique_ptr<Expression<ReturnType::INTEGER>> high; ///< More significant bit index, null if full signal or single bit indexing
    };

    /// Binary Operation: x"00" or 0b101010 ; 30 + 10 ; signal_name lls 3...
    /// @tparam R The return type of the expression
    template <ReturnType R>
    struct BinaryOpExpr : Expression<R>
    {
        std::string op; ///< Operator string, e.g. "+", "-", "and", "or", "lls", "rrs", etc.
        std::unique_ptr<ASTNode> left;  ///< Left operand of the binary operation
        std::unique_ptr<ASTNode> right; ///< Right operand of the binary operation
    };

    /// Unary Operation: -10 ; not signal_name...
    /// @tparam R The return type of the expression
    template <ReturnType R>
    struct UnaryOpExpr : Expression<R>
    {
        std::string op; ///< Operator string, e.g. "-", "not", etc.
        std::unique_ptr<ASTNode> operand; ///< Operand of the unary operation
    };

    /// Function call expression: unsigned(signal_name), to_integer(signal_name), etc...
    /// @tparam R The return type of the expression
    template <ReturnType R>
    struct FunctionCallExpr : Expression<R>
    {
        std::string functionName; ///< Name of the function being called
        std::vector<std::unique_ptr<ASTNode>> arguments; ///< Arguments passed to the function call
    };

    /// Plain integer constant, used for range bounds, shift amounts, etc.
    struct IntegerLiteralExpr final : Expression<ReturnType::INTEGER>
    {
        int64_t value = 0; ///< Value of the integer literal
    };

    /// x"00AA", "0101", '0', '1', 'X', 'Z' ... a STD_LOGIC / STD_LOGIC_VECTOR literal.
    /// Each bit's value is meaningful only where the corresponding unknownMask bit is 0.
    struct LogicLiteralExpr final : Expression<ReturnType::LOGIC>
    {
        uint64_t value = 0;             ///< 0/1 when mask bit is 0, undefined/high-Z when mask bit is 1.
        uint64_t unknownMask = 0;       ///< 1 = bit is X/Z, 0 = bit is known 0/1
        Pulse::bitWidth_t width = 0;    ///< Width of the logic literal in bits
    };

    /// Known attributes of a signal reference.
    enum class AttributeKind
    {
        Left,
        Right,
        Low,
        High,
        Length,
    };

    /// signal_name'left / 'right / 'low / 'high / 'length
    struct AttributeExpr final : Expression<ReturnType::INTEGER>
    {
        AttributeKind kind; ///< Kind of attribute being accessed
        std::unique_ptr<SignalReference> target; ///< Target signal reference for the attribute access
    };

    /// When-Else expression: a set of chained conditions and their corresponding values, with an optional default value.
    /// Evaluates to a logic value based on the first condition that evaluates to true.
    struct WhenElseExpr final : Expression<ReturnType::LOGIC>
    {
        /// A "when" branch
        struct Branch
        {
            std::unique_ptr<Expression<ReturnType::LOGIC>> value;       ///< Value expression to be returned if the condition evaluates to true
            std::unique_ptr<Expression<ReturnType::BOOLEAN>> condition; ///< Condition expression to be evaluated for this branch
        };

        std::vector<Branch> branches;   ///< List of "when" branches in the order they must be evaluated.
        std::unique_ptr<Expression<ReturnType::LOGIC>> defaultValue; ///< Default value expression to be returned if no conditions evaluate to true (optional)
    };

    // --------------------------------------------------------------------------------------------

    /// wait for <integer> ns;
    /// Pauses simulation for the given number of nanoseconds.
    struct WaitForStatement final : SequentialStatement
    {
        int64_t durationNs = 0; ///< Duration of the wait in nanoseconds
    };

    /// if/elsif/else statement inside a process.
    struct IfStatement final : SequentialStatement
    {
        /// A single if / elsif branch
        struct Branch
        {
            std::unique_ptr<Expression<ReturnType::BOOLEAN>> condition; ///< Condition of this branch
            std::vector<std::unique_ptr<SequentialStatement>> body;     ///< Statements to execute when the condition is true
        };

        std::vector<Branch> branches;                                    ///< if + zero or more elsif branches, in order
        std::vector<std::unique_ptr<SequentialStatement>> elseBody;      ///< Statements to execute in the else clause (may be empty)
    };

    /// A VHDL process statement.
    /// Processes contain a sensitivity list, local signal declarations,
    /// and a sequential body (signal assignments, if/else, wait for).
    struct ProcessStatement final : ASTNode
    {
        std::string label;                                              ///< Optional process label (empty if unlabeled)
        std::vector<std::string> sensitivityList;                       ///< Names of signals in the sensitivity list (may be empty)
        std::vector<std::unique_ptr<SequentialStatement>> body;         ///< Sequential statements in the process body
    };

    // --------------------------------------------------------------------------------------------

    [[nodiscard]]
    ASTRoot VHDLtoAST(Tokenizer& tokenizer);

} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_ASTBUILDER_H