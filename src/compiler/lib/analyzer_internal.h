#ifndef PULSE_VHDL_ANALYZER_INTERNAL_H
#define PULSE_VHDL_ANALYZER_INTERNAL_H

#include "analyzer.h"
#include "ast.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace Pulse::Parser
{
    /// Clones an Expression object, creating a deep copy of the expression tree.
    ExpressionPtr cloneExpression(const Expression* src);

    /// Clones a TypeSpec object, creating a deep copy of the type specification.
    TypeSpec cloneTypeSpec(const TypeSpec& src);

    /// Kinds of trackable symbols in the AST.
    enum class SymbolKind { 
        IPort, 
        OPort, 
        IOPort, 
        Signal,
        Component
    };

    struct SymbolInfo
    {
        SymbolKind kind;
        TypeSpec typeSpec;
        SourceLocation loc;

        SymbolInfo() = default;
        SymbolInfo(SymbolKind k, TypeSpec&& ts, SourceLocation l)
            : kind(k), typeSpec(std::move(ts)), loc(l) { }
        SymbolInfo(SymbolInfo&&) noexcept = default;
        SymbolInfo& operator=(SymbolInfo&&) noexcept = default;
        SymbolInfo(const SymbolInfo&) = delete;
        SymbolInfo& operator=(const SymbolInfo&) = delete;
    };

    struct Scope
    {
        std::unordered_map<std::string, SymbolInfo> symbols;
        std::vector<std::string> tags;

        Scope() = default;
        Scope(Scope&&) noexcept = default;
        Scope& operator=(Scope&&) noexcept = default;
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    /// Central context class that maintains state during semantic analysis.
    class AnalyzerContext
    {
        // -------- Scopes & Symbol Tables --------------------------------------------------------

        std::vector<Scope> m_scopes; /// Stack of scopes for symbol resolution.
        std::unordered_map<std::string, const EntityDeclaration*> m_entities;
        std::unordered_map<std::string, const ComponentDeclaration*> m_components;

        void pushScope();
        void popScope();

        /// Declare a new tag in the current scope.
        /// @throws ast_semantic_error if the tag already exists in the current scope.
        void declareTag(const std::string& tag, const ASTNode& node);
        
        /// Declare a new symbol in the current scope.
        /// @throws ast_semantic_error if the symbol already exists in the current scope.
        void declareSymbol(const std::string& name, SymbolKind kind, const TypeSpec& type, const ASTNode& node);

        /// Retrieve a symbol from the current scope.
        /// @throws ast_semantic_error if the symbol is not found in any active scope.
        const SymbolInfo& getSymbol(const std::string& name, const ASTNode& node) const;

        /// Check if a symbol exists in any active scope without throwing.
        bool hasSymbol(const std::string& name) const;

        // -------- Type Validation ---------------------------------------------------------------

        /// Returns true if typeName is a supported built-in type.
        bool isBuiltinType(const std::string& typeName) const;

        /// Returns true if typeName is a logic type (std_logic or std_logic_vector).
        bool isLogicType(const std::string& typeName) const;

        /// Returns true if both TypeSpecs are logic types.
        bool areBothLogic(const TypeSpec& a, const TypeSpec& b) const;

        /// Validates a TypeSpec (built-in type name, valid argument count and range bounds).
        /// @throws ast_semantic_error on invalid type specification.
        void checkTypeSpec(const TypeSpec& typeSpec);

        /// Returns true if `right` can be assigned to a context expecting `left`.
        bool areTypesCompatible(const TypeSpec& left, const TypeSpec& right) const;

        /// Extracts the bit-width from a std_logic_vector TypeSpec. Returns -1 if dynamic/unknown.
        int resolveVectorWidth(const TypeSpec& typeSpec) const;

        /// Builds a std_logic_vector TypeSpec with range (width - 1 downto 0).
        TypeSpec makeVectorType(int width, SourceLocation loc = {0, 0}) const;

        // -------- Expression Type Inference -----------------------------------------------------

        /// Recursively infer and validate the TypeSpec produced by an expression.
        TypeSpec exprType(const Expression* expr);

        TypeSpec exprTypeSymbol(const SymbolExpr* expr);
        TypeSpec exprTypeIntLit(const IntegerLiteralExpr* expr);
        TypeSpec exprTypeBoolLit(const BooleanLiteral* expr);
        TypeSpec exprTypeLogicLit(const LogicLiteralExpr* expr);
        TypeSpec exprTypeUnaryOp(const UnaryOpExpr* expr);
        TypeSpec exprTypeBinaryOp(const BinaryOpExpr* expr);
        TypeSpec exprTypeFuncCall(const FunctionCallExpr* expr);
        TypeSpec exprTypeAttribute(const AttributeExpr* expr);
        TypeSpec exprTypeWhenElse(const WhenElseExpr* expr);

        // Built-in function helpers
        TypeSpec exprTypeFuncUnsigned(const FunctionCallExpr* expr);
        TypeSpec exprTypeFuncSigned(const FunctionCallExpr* expr);
        TypeSpec exprTypeFuncToUnsigned(const FunctionCallExpr* expr);
        TypeSpec exprTypeFuncToSigned(const FunctionCallExpr* expr);
        TypeSpec exprTypeFuncToInteger(const FunctionCallExpr* expr);
        TypeSpec exprTypeFuncRisingEdge(const FunctionCallExpr* expr);
        TypeSpec exprTypeFuncFallingEdge(const FunctionCallExpr* expr);
        TypeSpec exprTypeFuncVectorCast(const FunctionCallExpr* expr);
        TypeSpec exprTypeSliceOrIndex(const FunctionCallExpr* expr);

        // -------- Lvalue & Assignment Validation ------------------------------------------------

        /// Returns true if expr is a modifiable lvalue.
        bool isLvalue(const Expression* expr) const;

        /// Returns true if an attribute is modifiable (currently all built-in attributes are read-only).
        bool isAttributeModifiable(const std::string& attributeName) const;

        /// Validates an assignment statement: checks lvalue and type compatibility.
        void analyzeAssignment(const Expression* left, const Expression* right, const ASTNode& node);

        // -------- Structural Analysis -----------------------------------------------------------

        /// Collects all top-level entities into m_entities.
        void collectEntities(const ASTRoot& root);

        /// Validates entity declaration and its port list.
        void analyzeEntity(const EntityDeclaration& entity);

        /// Validates architecture declaration, declarations region, and concurrent body.
        void analyzeArchitecture(const ArchitectureDeclaration& arch);

        /// Validates signal declaration and initial value.
        void checkSignalDeclaration(const SignalDeclaration& signal);

        /// Validates initial value constant expression.
        void checkInitialValue(const Expression* init, const TypeSpec& targetType, const ASTNode& node);

        // -------- Statement Analysis ------------------------------------------------------------

        void analyzeConcurrentStmt(const Statement& stmt);
        void analyzeSignalAssign(const SignalAssignment& assign);
        void analyzeWithClause(const WithClause& stmt);
        void analyzeCompInstantiation(const ComponentInstantiation& inst);
        void analyzeProcess(const ProcessStatement& proc);

        void analyzeSeqStmt(const Statement& stmt);
        void analyzeIfStmt(const IfStatement& stmt);
        void checkNoWaitStatements(const std::vector<std::unique_ptr<SequentialStatement>>& body, const ASTNode& proc);

    public:
        explicit AnalyzerContext();
        void analyze(const ASTRoot& root);
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_ANALYZER_INTERNAL_H