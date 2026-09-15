#ifndef PULSE_PARSER_LIB_AST_H
#define PULSE_PARSER_LIB_AST_H

#include "include/parser.h"

#include <unordered_map>
#include <initializer_list>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace Pulse::Parser
{
    /// A context for parsing tokens into an AST.
    struct ParseContext
    {
        Tokenizer& m_tokenizer;

        explicit ParseContext(Tokenizer& tok);
        ASTRoot parse();                                            /// Parse the entire token stream into an ASTRoot.

        // --------------------------------------------------------------------------------------------------------------------------------------------------------------

        const Token* peek(ssize_t offset = 0);                      /// Peek at a token at a specific offset from the current cursor position
        const Token* next();                                        /// Advance the cursor and return the token now under the cursor
        const Token* expect(std::string_view v);                    /// Expect a specific token value and advance the cursor
        const Token* expect(std::initializer_list<std::string_view> v); /// Expect one of several token values and advance the cursor
        const Token* maybe(std::string_view v);                     /// Expect an optional token value and advance the cursor if it matches.
        const Token* expectIdentifier();                            /// Expect an identifier token and advance the cursor
        const Token* skipUntil(std::string_view v);                 /// Skip tokens until a specific token value is found, then return that token
        const Token* skipUntil(std::string_view v, TokenType t);    /// Skip tokens until a specific token value is found, then return that token

        [[noreturn]] void error(std::string_view msg);              /// Report a parse error at the current token position and throw an exception

        // -------- Core ------------------------------------------------------------------------------------------------------------------------------------------------

        TypeSpec parseTypeSpec();                                   /// Parse a type specification. Cursor must be over the type name token.
        std::vector<PortDeclaration> parsePortList();               /// Parse a port list. Cursor must be over the "port" token.
        PortDeclaration parsePortDeclaration();                     /// Parse a single port declaration. Cursor must be over the identifier token of the port name.

        // -------- Root level structure --------------------------------------------------------------------------------------------------------------------------------
        
        std::unique_ptr<EntityDeclaration> parseEntity();           /// Parse an entity declaration. Cursor must be over the "entity" token.
        std::unique_ptr<ArchitectureDeclaration> parseArch();       /// Parse an architecture declaration. Cursor must be over the "architecture" token.
        std::vector<SignalDeclaration> parseSignalDeclaration();    /// Parse a signal declaration. Cursor must be over the "signal" token. (can return various signals of the same type)
        ComponentDeclaration parseComponentDeclaration();           /// Parse a component declaration. Cursor must be over the "component" token.
        
        // -------- Statements ------------------------------------------------------------------------------------------------------------------------------------------
        
        std::unique_ptr<SignalAssignment> parseSignalAssignment();  /// Parse a signal assignment statement. Cursor must be over the target signal reference.
        std::unique_ptr<WithClause> parseWithClause();              /// Parse a with-select statement. Cursor must be over the "with" token.
        
        /// Parse a component instantiation statement. Cursor must be over the "component" token. The instance name is provided as an argument.
        std::unique_ptr<ComponentInstantiation> parseComponentInstantiation(const std::string& instanceLabel);  
        
        /// Parse a process statement. Cursor must be over the "process" token.
        std::unique_ptr<ProcessStatement> parseProcess(const std::string& processLabel); 
        
        // -------- Expressions -----------------------------------------------------------------------------------------------------------------------------------------
        
        std::unique_ptr<WhenElseExpr> parseWhenElse();              /// Parse a when-select expression. Cursor must be over the "when" token.
        std::unique_ptr<WhenElseExpr> parseWhenElseSuffix(ExpressionPtr baseExpr); /// Wraps an already-parsed base expression into a WhenElse chain.
        std::unique_ptr<LogicLiteralExpr> parseLogicLiteral();      /// Parse a logic literal expression. Cursor must be over the literal token.
        std::unique_ptr<IntegerLiteralExpr> parseIntegerLiteral();  /// Parse an integer literal expression. Cursor must be over the literal token.
        std::unique_ptr<BooleanLiteral> parseBooleanLiteral();      /// Parse a boolean literal expression. Cursor must be over the "true" or "false" token.
        std::unique_ptr<UnaryOpExpr> parseUnaryOp();                /// Parse a unary operation. Cursor must be over the unary operator token.
        ExpressionPtr parseIdentifierOperand();                     /// Parse an identifier-based operand (Attribute, FunctionCall, or Symbol). Cursor must be over the identifier.
        ExpressionPtr parseExpression(bool chainWhens = true);      /// Parse an expression of any type. Cursor must be over the first token of the expression.
        ExpressionPtr parseExpressionWithPrecedence(int minPrecedence, bool chainWhens = true); /// Parse expression with precedence climbing.
        ExpressionPtr parseOperand(bool chainWhens = true);         /// Parse an operand without chaining it with neighboring operators.

        // -------- Process-related -------------------------------------------------------------------------------------------------------------------------------------
        
        /// Parse an if statement inside a process. Cursor must be over the "if" token.
        std::unique_ptr<IfStatement> parseIfStatement();
        
        /// Parse a wait statement inside a process. Cursor must be over the "wait" token.
        std::unique_ptr<SequentialStatement> parseWaitStatement();
    };

} // namespace Pulse::Parser

#endif // PULSE_PARSER_LIB_AST_H
