#include "parser_internal.h"

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

    namespace BitstringUtils
    {
        void parsePrefix(const std::string& prefix, int& explicitSize, std::string& typeName, char& base)
        {
            explicitSize = -1;
            typeName = "std_logic_vector";
            base = 'B';

            if (prefix.empty()) return;

            size_t idx = 0;

            if (std::isdigit(prefix[0]))
            {
                explicitSize = 0;
                while (idx < prefix.size() && std::isdigit(prefix[idx]))
                    explicitSize = explicitSize * 10 + (prefix[idx++] - '0');
            }

            if (idx < prefix.size())
            {
                char c = std::toupper(prefix[idx]);
                if (c == 'S')
                {
                    typeName = "signed";
                    idx++;
                }
                else if (c == 'U')
                {
                    typeName = "unsigned";
                    idx++;
                }
            }

            if (idx < prefix.size())
                base = std::toupper(prefix[idx]);
        }

        void parseBits(const std::string& bits, char base, uint64_t& val, uint64_t& msk, uint32_t& bitCount, ParseContext& ctx)
        {
            static const std::unordered_map<char, int> bitsPerDigit = { { 'B', 1 },{ 'O', 3 },{ 'X', 4 },{ 'D', 0 } };
            int stride = bitsPerDigit.count(base) ? bitsPerDigit.at(base) : 0;

            for (char c : bits)
            {
                if (c == '_') continue;

                char u = std::toupper(c);

                if (base == 'D')
                {
                    if (!std::isdigit(u)) ctx.error("Invalid character in decimal literal");
                    val = val * 10 + (u - '0');
                    continue;
                }

                val <<= stride;
                msk <<= stride;
                bitCount += stride;

                if (u == 'X')                                       msk |= (1ULL << stride) - 1;
                else if (u == 'Z') { val |= (1ULL << stride) - 1;   msk |= (1ULL << stride) - 1; }
                else if (u == '0' || u == '-'); // 0 or don't-care: no bits set
                else if (u == '1')                                  val |= 1;
                else if (std::isdigit(u))                           val |= (u - '0');
                else if (base == 'X' && u >= 'A' && u <= 'F')       val |= (u - 'A' + 10);
                else                                                ctx.error(std::string("Invalid character in literal: '") + c + "'");
            }

            if (base == 'D')
            {
                uint64_t temp = val;
                bitCount = 0;
                while (temp > 0) { temp >>= 1; bitCount++; }
                if (bitCount == 0) bitCount = 1; // val == 0 needs at least 1 bit
            }
        }

        void applyExplicitSize(uint64_t& val, uint64_t& msk, uint32_t& bitCount, int explicitSize)
        {
            if (explicitSize != -1)
                bitCount = static_cast<uint32_t>(explicitSize);
        }
    } // namespace BitstringUtils

    std::unique_ptr<LogicLiteralExpr> ParseContext::parseLogicLiteral()
    {
        using namespace BitstringUtils;

        const Token* tok = peek();
        if (!tok || (tok->type != TokenType::BitStringLiteral && tok->type != TokenType::CharacterLiteral))
            error("Expected a logic literal token");

        next();

        auto expr = std::make_unique<LogicLiteralExpr>();
        expr->source = { tok->line, tok->column };

        if (tok->type == TokenType::CharacterLiteral)
        {
            expr->width = 1;
            expr->typeName = "std_logic";

            char u = std::toupper(tok->value[1]);
            expr->value = (u == '1' || u == 'Z') ? 1 : 0;
            expr->mask  = (u == 'X' || u == 'Z') ? 1 : 0;
        }
        else
        {
            size_t pos = tok->value.find('"');
            if (pos == std::string::npos) error("Malformed bit string literal");

            std::string prefix = tok->value.substr(0, pos);
            std::string bits   = tok->value.substr(pos + 1, tok->value.size() - pos - 2);

            int explicitSize; std::string typeName; char base;
            parsePrefix(prefix, explicitSize, typeName, base);

            uint64_t val = 0, msk = 0;
            uint32_t bitCount = 0;

            parseBits(bits, base, val, msk, bitCount, *this);
            applyExplicitSize(val, msk, bitCount, explicitSize);

            if (bitCount > 64) error("Literal width exceeds 64 bits");

            uint64_t finalMask = (bitCount == 64) ? ~0ULL : (1ULL << bitCount) - 1;

            expr->value   = val & finalMask;
            expr->mask    = msk & finalMask;
            expr->width   = bitCount;
            expr->typeName = typeName;
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