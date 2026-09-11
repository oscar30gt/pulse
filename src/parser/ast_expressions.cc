#include "ast_internal.h"

namespace Pulse::Parser
{
    // Determine the precedence (binding power) of an operator
    // Lower number = lower precedence (binds less tightly)
    static int getOperatorPrecedence(const std::string& op)
    {
        if (op == "or" || op == "nor")
            return 0;
        if (op == "and" || op == "nand" || op == "xor" || op == "xnor")
            return 1;
        if (op == "=" || op == "/=" || op == "<" || op == "<=" || op == ">" || op == ">=")
            return 2;
        if (op == "to" || op == "downto" || op == "sll" || op == "srl" || op == "sra" || op == "rol" || op == "ror")
            return 3;
        if (op == "+" || op == "-" || op == "&")
            return 4;
        if (op == "*")
            return 5;
        return -1; // Not an operator
    }

    // Check if operator is right-associative
    // Right-associative means that in a chain of the same operator, evaluation starts from the right.
    // For example, exponentiation is right-associative: a ^ b ^ c is evaluated as a ^ (b ^ c).
    static bool isRightAssociative(const std::string& op)
    {
        return false; // All supported operators are left-associative.
        // As more syntax is added, this function can be updated to handle right-associative operators.
    }

    std::unique_ptr<LogicLiteralExpr> ParseContext::parseLogicLiteral()
    {
        const Token* tok = peek();
        if (!tok || (tok->type != TokenType::BitStringLiteral && tok->type != TokenType::CharacterLiteral))
            error("Expected a logic literal token");

        next(); // Consume the literal token

        auto expr = std::make_unique<LogicLiteralExpr>();
        expr->source = { tok->line, tok->column };

        if (tok->type == TokenType::CharacterLiteral)
        {
            char bit = tok->value[1]; // Extract the bit character (assuming format is '0', '1', 'X', or 'Z')
            expr->value = (bit == '1' || bit == 'Z') ? 1 : 0;
            expr->mask = (bit == 'X' || bit == 'Z') ? 1 : 0;
            expr->width = 1;
        }

        // BitStringLiteral
        else
        {
            std::string prefix; // Radix prefix (e.g., "b" on b"1010")
            std::string bits; // The actual bit string (e.g., "1010" in b"1010")
            std::string tokValue = tok->value;

            size_t pos = tokValue.find('"');
            prefix = pos == std::string::npos ? "b" : tokValue.substr(0, pos); // Default to binary if no prefix is found
            bits = tokValue.substr(pos + 1, tokValue.size() - 2); // Remove prefix and quotes

            for (size_t i = 0; i < bits.size(); ++i)
            {
                char bit = bits[i];
                expr->value <<= 1;
                expr->mask <<= 1;

                if (bit == '1' || bit == 'Z')
                    expr->value |= 1;
                else if (bit == 'X' || bit == 'Z')
                    expr->mask |= 1;

                expr->width++;
            }
        }

        return expr;
    }

    std::unique_ptr<IntegerLiteralExpr> ParseContext::parseIntegerLiteral()
    {
        auto* tok = next(); // Consume the numeric literal token
        auto expr = std::make_unique<IntegerLiteralExpr>();
        expr->source = { tok->line, tok->column };
        expr->value = std::stoll(tok->value);
        return expr;
    }

    std::unique_ptr<BooleanLiteral> ParseContext::parseBooleanLiteral()
    {
        auto* tok = next(); // Consume "true" or "false"
        auto expr = std::make_unique<BooleanLiteral>();
        expr->source = { tok->line, tok->column };
        expr->value = (tok->value == "true");
        return expr;
    }

    std::unique_ptr<UnaryOpExpr> ParseContext::parseUnaryOp()
    {
        auto* tok = next(); // Consume "not" or "-"
        auto operand = parseOperand(false);
        auto expr = std::make_unique<UnaryOpExpr>();
        expr->source = { tok->line, tok->column };
        expr->op = tok->value;
        expr->operand = std::move(operand);
        return expr;
    }

    ExpressionPtr ParseContext::parseIdentifierOperand()
    {
        auto* tok = next(); // Consume the identifier token
        std::string idName = tok->value;

        // Identifier followed by ' → AttributeExpr
        if (peek() && peek()->value == "'")
        {
            next(); // consume '
            auto* attrTok = expectIdentifier();
            auto target = std::make_unique<SymbolExpr>();
            target->source = { tok->line, tok->column };
            target->name = idName;

            auto expr = std::make_unique<AttributeExpr>();
            expr->source = { tok->line, tok->column };
            expr->attributeName = attrTok->value;
            expr->target = std::move(target);
            return expr;
        }

        // Identifier followed by ( → FunctionCallExpr
        if (peek() && peek()->value == "(")
        {
            next(); // consume (
            auto expr = std::make_unique<FunctionCallExpr>();
            expr->source = { tok->line, tok->column };
            expr->functionName = idName;

            if (peek()->value != ")")
            {
                do
                {
                    expr->arguments.push_back(parseExpression(true));
                } while (maybe(","));
            }

            expect(")");
            return expr;
        }

        // Plain identifier → SymbolExpr
        auto expr = std::make_unique<SymbolExpr>();
        expr->source = { tok->line, tok->column };
        expr->name = idName;
        return expr;
    }

    std::unique_ptr<WhenElseExpr> ParseContext::parseWhenElseSuffix(ExpressionPtr baseExpr)
    {
        auto whenExpr = std::make_unique<WhenElseExpr>();
        whenExpr->source = baseExpr->source;
        
        // baseExpr is the value to return if the condition evaluates to true
        whenExpr->trueValue = std::move(baseExpr);
        
        next(); // Consume "when"
        
        // Parse condition (a condition should not absorb chained whens)
        whenExpr->condition = parseExpression(false);
        
        // Parse the else branch recursively
        if (peek() && peek()->value == "else")
        {
            next(); // consume "else"
            // Pass true so that if it's another 'when', it parses the rest of the chain seamlessly
            whenExpr->falseValue = parseExpression(true);
        }
        else
        {
            whenExpr->falseValue = nullptr;
        }
        
        return whenExpr;
    }

    ExpressionPtr ParseContext::parseOperand(bool chainWhens)
    {
        const Token* tok = peek();
        if (!tok)
            error("Unexpected end of input while parsing operand");

        ExpressionPtr expr;

        // Route to specific parsing methods based on token
        if (tok->type == TokenType::NumericLiteral)
            expr = parseIntegerLiteral();
        else if (tok->type == TokenType::BitStringLiteral || tok->type == TokenType::CharacterLiteral)
            expr = parseLogicLiteral();
        else if (tok->value == "true" || tok->value == "false")
            expr = parseBooleanLiteral();
        else if (tok->value == "not" || tok->value == "-")
            expr = parseUnaryOp();
        else if (tok->type == TokenType::Identifier)
            expr = parseIdentifierOperand();
        else if (tok->value == "(")
        {
            next(); // Consume '('
            // Nested parentheses always allow 'when' chains
            expr = parseExpression(true);
            expect(")"); // Consume the closing ')'
        }
        else
            error("Unexpected token in operand: " + tok->value);

        // We DO NOT evaluate 'when' at the operand level anymore.
        // This ensures proper binding power for operators (e.g., A + B when C binds as (A + B) when C).
        return expr;
    }

    ExpressionPtr ParseContext::parseExpression(bool chainWhens)
    {
        auto expr = parseExpressionWithPrecedence(0, chainWhens);

        // Check for "when" at the expression root level to absorb into WhenElseExpr
        if (chainWhens && peek() && peek()->value == "when")
        {
            return parseWhenElseSuffix(std::move(expr));
        }

        return expr;
    }

    ExpressionPtr ParseContext::parseExpressionWithPrecedence(int minPrecedence, bool chainWhens)
    {
        auto left = parseOperand(chainWhens);

        while (peek() && getOperatorPrecedence(peek()->value) >= minPrecedence)
        {
            auto* opTok = next(); // Consume operator
            std::string op = opTok->value;
            int opPrec = getOperatorPrecedence(op);

            // For right-associative operators, use the same precedence for recursion
            // For left-associative, use precedence + 1
            int nextMinPrec = isRightAssociative(op) ? opPrec : opPrec + 1;
            auto right = parseExpressionWithPrecedence(nextMinPrec, chainWhens);

            auto binExpr = std::make_unique<BinaryOpExpr>();
            binExpr->source = { opTok->line, opTok->column };
            binExpr->op = op;
            binExpr->left = std::move(left);
            binExpr->right = std::move(right);

            left = std::move(binExpr);
        }

        return left;
    }

} // namespace Pulse::Parser