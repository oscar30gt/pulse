#ifndef PULSE_VHDL_AST_H
#define PULSE_VHDL_AST_H

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <unordered_map>

namespace Pulse::Parser
{
    /// Location of a certain node in the source file.
    struct SourceLocation
    {
        size_t line;          /// Line number (1-based)
        size_t column;        /// Column number (1-based)
    };

    // --------------------------------------------------------------------------------------------

    /// Base class for all AST Nodes to allow proper polymorphism without slicing.
    struct ASTNode
    {
        virtual ~ASTNode() = default;
        SourceLocation source; /// Location of the node in the source file
    };

    /// Root node containing top-level entities and architectures
    struct ASTRoot final : ASTNode
    {
        /// Top-level entities and architectures in the order they were parsed.
        std::vector<std::unique_ptr<ASTNode>> children;

        /// Print the AST tree to stdout in a human-readable format.
        /// This is primarily for debugging purposes.
        void print() const;
    };

    // --------------------------------------------------------------------------------------------
    
    struct Expression : ASTNode { };                    /// Base class for all evaluable expressions.
    using ExpressionPtr = std::unique_ptr<Expression>;  /// Shorthand for a unique pointer to an Expression.

    /// Type specification for signals and ports, including range information if applicable.
    struct TypeSpec final : ASTNode
    {   
        std::string typeName;               /// Name of the type (e.g., "std_logic", "integer", etc.)  
        std::vector<ExpressionPtr> args;    /// Optional args such as range expressions for array types (e.g., "std_logic_vector(7 downto 0)")
    };

    /// Declaration of a port in VHDL.
    /// Range flattenning for array ports is applied so downto/to is always normalized to a width.
    struct PortDeclaration final : ASTNode
    {
        std::string portName;   /// Name of the port
        TypeSpec typeSpec;      /// Type specification of the port, including range information
        bool isInput : 1;       /// True if the port is an input
        bool isOutput : 1;      /// True if the port is an output
    };

    /// Declaration of an entity in VHDL.
    /// Entities support multiple ports.
    struct EntityDeclaration final : ASTNode
    {
        std::string name;                   /// Name of the entity
        std::vector<PortDeclaration> ports; /// Ports of the entity
    };

    /// Declaration of a signal within an architecture.
    struct SignalDeclaration final : ASTNode
    {
        std::string name;                               /// Name of the signal
        TypeSpec typeSpec;                              /// Type specification of the signal, including range information
        ExpressionPtr initialValue = nullptr;           /// Optional initial value for the signal [must be known at compile time]
    };

    /// Declaration of a component within an architecture.
    struct ComponentDeclaration final : ASTNode
    {
        std::string name;                   /// Name of the component
        std::vector<PortDeclaration> ports; /// Ports of the component
    };


    // --------------------------------------------------------------------------------------------

    struct Statement : ASTNode { };                     /// Base class for all statements (concurrent or sequential).
    using SequentialStatement = Statement;              /// Sequential statements that can appear inside a process body.
    using ConcurrentStatement = Statement;              /// Combinational statements that can appear inside an architecture.

    /// Declaration of an architecture in VHDL.
    /// Architectures support multiple signals, components, assignments,
    /// instantiations, and processes.
    struct ArchitectureDeclaration final : ASTNode
    {
        std::string entityName;                         /// Entity name that this architecture is associated with
        std::string name;                               /// Name of this architecture

        // Declarations region
        std::vector<SignalDeclaration> signals;         /// Signals declared in the architecture
        std::vector<ComponentDeclaration> components;   /// Components declared in the architecture

        /// Concurrent region of the architecture (between begin and end) in the order statements were parsed.
        std::vector<std::unique_ptr<ConcurrentStatement>> body;
    };

    // --------------------------------------------------------------------------------------------
    
    /// A reference to a known symbol (signal, port, component, etc.)
    struct SymbolExpr final : Expression
    {
        std::string name;     /// Name of the signal being referenced
    };
    
    /// Assignment of a value to a signal within an architecture
    struct SignalAssignment final : Statement
    {
        ExpressionPtr target;       /// Target signal reference for the assignment
        ExpressionPtr value;        /// Value expression to be assigned to the target
    };

    struct WithClause final : Statement
    {
        ExpressionPtr selector;                                         /// Selector expression: with (selector) select
        std::vector<std::pair<ExpressionPtr, ExpressionPtr>> choices;   /// List of choice pairs: (first) when (second)
        ExpressionPtr defaultValue;                                     /// Default value expression if no choices match (optional)
    };

    /// Instantiation of a component within an architecture
    struct ComponentInstantiation final : ConcurrentStatement
    {
        std::string instanceName;                                       /// Name of the component instance
        std::string componentName;                                      /// Name of the component being instantiated
        std::vector<std::pair<std::string, SymbolExpr>> portMap;   /// Mapping of component ports to signals in the architecture
    };

    /// Process statement within an architecture
    struct ProcessStatement final : ConcurrentStatement
    {
        std::string label;                                      /// Optional process label (empty if unlabeled)
        std::vector<std::string> sensitivityList;               /// Names of signals in the sensitivity list (may be empty)
        std::vector<std::unique_ptr<SequentialStatement>> body; /// Sequential statements in the process body
    };

    // --------------------------------------------------------------------------------------------    

    /// Binary expression: e.g., "a + b", "a and b", "a lls b", etc.
    struct BinaryOpExpr final : Expression
    {
        std::string op;       /// Operator string, e.g. "+", "-", "and", "or", "lls", "rrs", etc.
        ExpressionPtr left;   /// Left operand of the binary operation
        ExpressionPtr right;  /// Right operand of the binary operation
    };

    /// Unary expression: e.g., "-a", "not a", etc.
    struct UnaryOpExpr final : Expression
    {
        std::string op;           /// Operator string, e.g. "-", "not", etc.
        ExpressionPtr operand;    /// Operand of the unary operation
    };

    /// Function call expression: e.g., "unsigned(signal_name)", "to_integer(signal_name)", etc.
    struct FunctionCallExpr final : Expression
    {
        std::string functionName;             /// Name of the function being called
        std::vector<ExpressionPtr> arguments; /// Arguments passed to the function call
    };

    /// Plain integer constant.
    struct IntegerLiteralExpr final : Expression
    {
        int64_t value = 0; /// Value of the integer literal
    };

    /// Plain boolean constant: true or false.
    struct BooleanLiteral final : Expression
    {
        bool value = false; /// Value of the boolean literal (true or false)
    };

    /// x"00AA", "0101", '0', '1', 'X', 'Z' ... STD_LOGIC / STD_LOGIC_VECTOR literal.
    /// Each bit's value is meaningful only where the corresponding unknownMask bit is 0.
    struct LogicLiteralExpr final : Expression
    {
        std::string typeName;       /// "std_logic", "std_logic_vector", "signed", or "unsigned"
        uint64_t value = 0;         /// 0/1 when mask bit is 0, undefined/high-Z when mask bit is 1.
        uint64_t mask = 0;          /// 1 = bit is unknown X/Z, 0 = bit is known 0/1
        uint8_t width : 7 = 0;      /// Width of the logic literal in bits
    };

    /// Accessing an attribute of a signal, such as "signal_name'left" or "signal_name'event".
    struct AttributeExpr final : Expression
    {
        std::string attributeName;              /// Name of the attribute being accessed (e.g., "left", "high", "length", "event", etc.)
        std::unique_ptr<SymbolExpr> target;     /// Target signal reference for the attribute access
    };

    // --------------------------------------------------------------------------------------------    

    /// When-Else expression: a ternary-like expression that evaluates to one value if a condition
    /// is true, and another value if the condition is false.
    struct WhenElseExpr final : Expression
    {   
        ExpressionPtr trueValue;        /// Value expression to be returned if the condition evaluates to true.
        ExpressionPtr condition;        /// Boolean expression to be evaluated for this branch.
        ExpressionPtr falseValue;       /// Default value expression to be returned if no conditions evaluate to true (optional).
    };

    // --------------------------------------------------------------------------------------------

    /// wait for <integer> [fs | ps | ns | us | ms];
    /// Pauses simulation for the given duration (stored in femtoseconds).
    struct WaitForStatement final : SequentialStatement
    {
        uint64_t durationFs = 0; /// Duration of the wait in femtoseconds
    };

    struct WaitForeverStatement final : SequentialStatement { };

    /// if/elsif/else statement inside a process.
    struct IfStatement final : SequentialStatement
    {
        /// An if / elsif branch
        struct Branch
        {
            ExpressionPtr condition;                                    /// Condition of this branch
            std::vector<std::unique_ptr<SequentialStatement>> body;     /// Statements to execute when the condition is true
        };

        std::vector<Branch> branches;                                    /// if + zero or more elsif branches, in order
        std::vector<std::unique_ptr<SequentialStatement>> elseBody;      /// Statements to execute in the else clause (may be empty)
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_AST_H