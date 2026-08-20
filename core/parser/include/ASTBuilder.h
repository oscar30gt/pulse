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
    enum class ReturnType
    {
        LOGIC,
        INTEGER,
        SIGNED,
        UNSIGNED,
        BOOLEAN,
    };

    ///////////////////////////////////////////////////////////////////////////

    /// Base class for all AST Nodes to allow proper polymorphism without slicing.
    struct ASTNode { virtual ~ASTNode() = default; };

    /// Root node containing top-level entities and architectures
    struct RootNode final : ASTNode
    {
        std::vector<std::unique_ptr<ASTNode>> children;
    };

    ///////////////////////////////////////////////////////////////////////////

    // Flattened port declaration
    struct PortDeclaration : ASTNode
    {
        std::string portName;
        Pulse::bitWidth_t width;
        bool isInput;
    };

    struct EntityDeclaration : ASTNode
    {
        std::string entityName;
        std::vector<std::unique_ptr<PortDeclaration>> ports;
    };

    struct SignalDeclaration;
    struct ComponentDeclaration;
    struct SignalAssignment;
    struct ComponentInstantiation;

    struct ArchitectureDeclaration : ASTNode
    {
        std::string architectureName;
        std::string entityName;

        std::vector<std::unique_ptr<SignalDeclaration>> signals;
        std::vector<std::unique_ptr<ComponentDeclaration>> components;
        std::vector<std::unique_ptr<SignalAssignment>> assignments;
        std::vector<std::unique_ptr<ComponentInstantiation>> instantiations;
    };

    struct SignalDeclaration : ASTNode
    {
        std::string signalName;
        Pulse::bitWidth_t width;
        uint64_t initialValue;
    };

    struct ComponentDeclaration : ASTNode
    {
        std::string componentName;
        std::vector<std::unique_ptr<PortDeclaration>> ports;
    };

    struct SignalReference;

    // Component instantiation in architecture
    struct ComponentInstantiation : ASTNode
    {
        std::string instanceName;
        std::string componentName;
        std::unordered_map<std::string, std::unique_ptr<SignalReference>> portMaps;
    };

    ///////////////////////////////////////////////////////////////////////////

    /// Base class for all evaluable expressions.
    template <ReturnType R>
    struct Expression : ASTNode
    {
        ReturnType returnType = R;
    };

    // signal_name(4 downto 0) <= x"00AA"
    struct SignalAssignment final : ASTNode
    {
        std::unique_ptr<SignalReference> target;
        std::unique_ptr<Expression<ReturnType::LOGIC>> value;
    };

    // signal_name ; signal_name(4 downto 0) ; signal_name(3) ; signal_name(7 to 4)...
    // low/high are always stored FLATTENED (bit 0 = the internal LSB position, see
    // ASTBuilder.cc for the normalization rules). Both null => full signal reference.
    // Only 'low' set => single-bit index. Both set => range (high/low may come out
    // "inverted", i.e. high < low, if the access direction differs from how the
    // signal was declared).
    struct SignalReference final : Expression<ReturnType::LOGIC>
    {
        std::string signalName;
        std::unique_ptr<Expression<ReturnType::INTEGER>> low;  // Null if full signal
        std::unique_ptr<Expression<ReturnType::INTEGER>> high; // Null if full signal or single bit indexing
    };

    // x"00" or 0b101010 ; 30 + 10 ; signal_name lls 3...
    template <ReturnType R>
    struct BinaryOpExpr : Expression<R>
    {
        std::string op;
        std::unique_ptr<ASTNode> left;
        std::unique_ptr<ASTNode> right;
    };

    template <ReturnType R>
    struct UnaryOpExpr : Expression<R>
    {
        std::string op;
        std::unique_ptr<ASTNode> operand;
    };

    template <ReturnType R>
    struct FunctionCallExpr : Expression<R>
    {
        std::string functionName;
        std::vector<std::unique_ptr<ASTNode>> arguments;
    };

    // Plain integer constant, used for range bounds, shift amounts, etc.
    struct IntegerLiteralExpr final : Expression<ReturnType::INTEGER>
    {
        int64_t value = 0;
    };

    // x"00AA", "0101", '0', '1', 'X', 'Z' ... a STD_LOGIC / STD_LOGIC_VECTOR literal.
    // Each bit's value is meaningful only where the corresponding unknownMask bit is 0.
    struct LogicLiteralExpr final : Expression<ReturnType::LOGIC>
    {
        uint64_t value = 0;
        uint64_t unknownMask = 0; // 1 = bit is X/Z/unknown, 0 = bit is known 0/1
        Pulse::bitWidth_t width = 0;
    };

    enum class AttributeKind
    {
        Left,
        Right,
        Low,
        High,
        Length,
    };

    // signal_name'left / 'right / 'low / 'high / 'length
    struct AttributeExpr final : Expression<ReturnType::INTEGER>
    {
        AttributeKind kind;
        std::unique_ptr<SignalReference> target;
    };

    // value_expr [when condition else value_expr when condition ...] else default_value_expr
    struct WhenElseExpr final : Expression<ReturnType::LOGIC>
    {
        struct Branch
        {
            std::unique_ptr<Expression<ReturnType::LOGIC>> value;
            std::unique_ptr<Expression<ReturnType::BOOLEAN>> condition;
        };

        std::vector<Branch> branches;
        std::unique_ptr<Expression<ReturnType::LOGIC>> defaultValue;
    };

    class ASTBuilder
    {
        RootNode root;

    public:
        ASTBuilder(Tokenizer& tokenizer);
        ~ASTBuilder() = default;

        RootNode& getRoot();
    };
} // namespace Pulse::Parser::VHDL

#endif // PULSE_VHDL_ASTBUILDER_H