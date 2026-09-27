#include "parser_impl.h"

#include <optional>

namespace Pulse::Parser
{
    namespace
    {
        struct BinarySpelling { std::string_view text; BinaryOperator op; };
        struct UnarySpelling  { std::string_view text; UnaryOperator op; };

        // One table per precedence level of VHDL-2008 (LRM 9.2), lowest first.
        constexpr BinarySpelling logicalOperators[] = {
            { "and", BinaryOperator::And }, { "or", BinaryOperator::Or }, { "nand", BinaryOperator::Nand },
            { "nor", BinaryOperator::Nor }, { "xor", BinaryOperator::Xor }, { "xnor", BinaryOperator::Xnor },
        };
        constexpr BinarySpelling relationalOperators[] = {
            { "=", BinaryOperator::Eq },        { "/=", BinaryOperator::Neq },       { "<", BinaryOperator::Lt },
            { "<=", BinaryOperator::Le },       { ">", BinaryOperator::Gt },         { ">=", BinaryOperator::Ge },
            { "?=", BinaryOperator::MatchEq },  { "?/=", BinaryOperator::MatchNeq }, { "?<", BinaryOperator::MatchLt },
            { "?<=", BinaryOperator::MatchLe }, { "?>", BinaryOperator::MatchGt },   { "?>=", BinaryOperator::MatchGe },
        };
        constexpr BinarySpelling shiftOperators[] = {
            { "sll", BinaryOperator::Sll }, { "srl", BinaryOperator::Srl }, { "sla", BinaryOperator::Sla },
            { "sra", BinaryOperator::Sra }, { "rol", BinaryOperator::Rol }, { "ror", BinaryOperator::Ror },
        };
        constexpr BinarySpelling addingOperators[] = {
            { "+", BinaryOperator::Add }, { "-", BinaryOperator::Sub }, { "&", BinaryOperator::Concat },
        };
        constexpr BinarySpelling multiplyingOperators[] = {
            { "*", BinaryOperator::Mul }, { "/", BinaryOperator::Div }, { "mod", BinaryOperator::Mod }, { "rem", BinaryOperator::Rem },
        };
        constexpr UnarySpelling factorOperators[] = {
            { "abs", UnaryOperator::Abs },         { "not", UnaryOperator::Not },
            { "and", UnaryOperator::ReduceAnd },   { "or", UnaryOperator::ReduceOr },   { "nand", UnaryOperator::ReduceNand },
            { "nor", UnaryOperator::ReduceNor },   { "xor", UnaryOperator::ReduceXor }, { "xnor", UnaryOperator::ReduceXnor },
        };

        /// The operator `token` spells in `table`, if it is an operator token of that level.
        template <typename Spelling, size_t N>
        auto lookup(const Spelling (&table)[N], const Token* token) -> std::optional<decltype(table[0].op)>
        {
            if (token->type != TokenType::Operator)
                return std::nullopt;
            for (const Spelling& entry : table)
                if (entry.text == token->value)
                    return entry.op;
            return std::nullopt;
        }

        ExpressionPtr makeBinary(BinaryOperator op, ExpressionPtr left, ExpressionPtr right)
        {
            auto binary = std::make_unique<BinaryOpExpr>();
            binary->source = left->source;
            binary->op = op;
            binary->left = std::move(left);
            binary->right = std::move(right);
            return binary;
        }

        ExpressionPtr makeUnary(const Token* token, UnaryOperator op, ExpressionPtr operand)
        {
            auto unary = std::make_unique<UnaryOpExpr>();
            unary->source = locationOf(token);
            unary->op = op;
            unary->operand = std::move(operand);
            return unary;
        }

        bool isNonAssociative(BinaryOperator op)
        {
            return op == BinaryOperator::Nand || op == BinaryOperator::Nor;
        }
    } // anonymous namespace

    // ---- expression ::= ?? primary | logical_expression -----------------------------------------

    ExpressionPtr Parser::parseExpression()
    {
        NestingGuard guard(*this);
        if (tokens.at("??"))
            return parseCondition();
        return parseLogical();
    }

    ExpressionPtr Parser::parseCondition()
    {
        const Token* op = tokens.expect("??");
        return makeUnary(op, UnaryOperator::Condition, parsePrimary());
    }

    // ---- Logical: relation { and relation }, relation [ nand relation ] -------------------------

    ExpressionPtr Parser::parseLogical()
    {
        ExpressionPtr left = parseRelation();
        std::optional<BinaryOperator> op = lookup(logicalOperators, tokens.peek());
        if (!op)
            return left;
        return parseLogicalChain(std::move(left), *op);
    }

    /// VHDL gives the logical operators no precedence among themselves: a chain must repeat one operator,
    /// and `nand` / `nor` cannot be chained at all.
    ExpressionPtr Parser::parseLogicalChain(ExpressionPtr left, BinaryOperator op)
    {
        ChainScope chain(*this);
        while (std::optional<BinaryOperator> next = lookup(logicalOperators, tokens.peek()))
        {
            chain.extend();
            if (*next != op)
                tokens.fail("Mixing '" + std::string(toString(op)) + "' and '" + toString(*next) + "' requires parentheses");
            tokens.next();
            left = makeBinary(op, std::move(left), parseRelation());
            if (isNonAssociative(op) && lookup(logicalOperators, tokens.peek()))
                tokens.fail("'" + std::string(toString(op)) + "' cannot be chained; use parentheses");
        }
        return left;
    }

    // ---- relation ::= shift [ relational_operator shift ] ---------------------------------------

    ExpressionPtr Parser::parseRelation()
    {
        ExpressionPtr left = parseShift();
        std::optional<BinaryOperator> op = lookup(relationalOperators, tokens.peek());
        if (!op)
            return left;
        tokens.next();
        return makeBinary(*op, std::move(left), parseShift());
    }

    // ---- shift_expression ::= simple [ shift_operator simple ] ----------------------------------

    ExpressionPtr Parser::parseShift()
    {
        ExpressionPtr left = parseSimpleExpression();
        std::optional<BinaryOperator> op = lookup(shiftOperators, tokens.peek());
        if (!op)
            return left;
        tokens.next();
        return makeBinary(*op, std::move(left), parseSimpleExpression());
    }

    // ---- simple_expression ::= [ sign ] term { adding_operator term } ---------------------------

    ExpressionPtr Parser::parseSimpleExpression()
    {
        ExpressionPtr left = parseSignedTerm();
        ChainScope chain(*this);
        while (std::optional<BinaryOperator> op = lookup(addingOperators, tokens.peek()))
        {
            chain.extend();
            tokens.next();
            left = makeBinary(*op, std::move(left), parseTerm());
        }
        return left;
    }

    ExpressionPtr Parser::parseSignedTerm()
    {
        if (const Token* plus = tokens.accept("+"))
            return makeUnary(plus, UnaryOperator::Plus, parseTerm());
        if (const Token* minus = tokens.accept("-"))
            return makeUnary(minus, UnaryOperator::Minus, parseTerm());
        return parseTerm();
    }

    // ---- term ::= factor { multiplying_operator factor } ----------------------------------------

    ExpressionPtr Parser::parseTerm()
    {
        ExpressionPtr left = parseFactor();
        ChainScope chain(*this);
        while (std::optional<BinaryOperator> op = lookup(multiplyingOperators, tokens.peek()))
        {
            chain.extend();
            tokens.next();
            left = makeBinary(*op, std::move(left), parseFactor());
        }
        return left;
    }

    // ---- factor ::= primary [ ** primary ] | abs primary | not primary | logical_operator primary

    ExpressionPtr Parser::parseFactor()
    {
        if (std::optional<UnaryOperator> op = lookup(factorOperators, tokens.peek()))
            return parseUnaryFactor(*op);

        ExpressionPtr base = parsePrimary();
        if (!tokens.accept("**"))
            return base;
        return makeBinary(BinaryOperator::Pow, std::move(base), parsePrimary());
    }

    ExpressionPtr Parser::parseUnaryFactor(UnaryOperator op)
    {
        const Token* token = tokens.next();
        return makeUnary(token, op, parsePrimary());
    }

    // ---- Primaries ------------------------------------------------------------------------------

    ExpressionPtr Parser::parsePrimary()
    {
        switch (tokens.peek()->type)
        {
            case TokenType::NumericLiteral:   return parseNumericLiteral();
            case TokenType::CharacterLiteral: return parseCharacterLiteral();
            case TokenType::Identifier:       return parseName();
            case TokenType::StringLiteral:    return tokens.at("(", 1) ? parseName() : parseStringLiteral();
            default:                          break;
        }

        if (tokens.at("("))
            return parseAggregateOrParenthesized();
        if (tokens.at("<<"))
            return parseName();
        tokens.expected("an expression");
    }

} // namespace Pulse::Parser
