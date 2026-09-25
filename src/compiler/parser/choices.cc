#include "parser_impl.h"

namespace Pulse::Parser
{
    // ---- Choices --------------------------------------------------------------------------------

    /// `choice { | choice }`
    ExpressionPtr Parser::parseChoices()
    {
        return parseChoicesFrom(parseChoice());
    }

    ExpressionPtr Parser::parseChoicesFrom(ExpressionPtr first)
    {
        auto list = std::make_unique<ChoiceListExpr>();
        list->source = first->source;
        list->alternatives.push_back(std::move(first));
        while (tokens.accept("|"))
            list->alternatives.push_back(parseChoice());
        return list;
    }

    /// `others`, a discrete range or an expression.
    ExpressionPtr Parser::parseChoice()
    {
        if (const Token* others = tokens.accept("others"))
            return node<OthersExpr>(others);
        return parseDiscreteRange();
    }

    // ---- Ranges ---------------------------------------------------------------------------------

    /// `type_mark range r` (a subtype indication), `a to b`, `a downto b`, or an expression (a range attribute,
    /// a type mark, or, where the caller allows it, any value).
    ExpressionPtr Parser::parseDiscreteRange()
    {
        if (tokens.atType(TokenType::Identifier) && tokens.at("range", 1))
            return parseTypeSpec();
        return parseRangeOrExpression();
    }

    ExpressionPtr Parser::parseRangeOrExpression()
    {
        return finishRange(parseExpression());
    }

    /// `simple_expression to|downto simple_expression`, or a range attribute such as `v'range`.
    ExpressionPtr Parser::parseRange()
    {
        return finishRange(parseSimpleExpression());
    }

    ExpressionPtr Parser::finishRange(ExpressionPtr low)
    {
        const Token* direction = tokens.accept("to");
        if (!direction)
            direction = tokens.accept("downto");
        if (!direction)
            return low;

        auto range = std::make_unique<BinaryOpExpr>();
        range->source = low->source;
        range->op = direction->value == "to" ? BinaryOperator::To : BinaryOperator::Downto;
        range->left = std::move(low);
        range->right = parseSimpleExpression();
        return range;
    }

    // ---- Aggregates -----------------------------------------------------------------------------

    /// `( element {, element} )`. A single positional element is a parenthesized expression and is returned
    /// as that expression; anything else is an AggregateExpr.
    ExpressionPtr Parser::parseAggregateOrParenthesized()
    {
        auto aggregate = node<AggregateExpr>(tokens.peek());
        parseParenthesized([&] { aggregate->elements.push_back(parseAggregateElement()); });

        const bool parenthesized = aggregate->elements.size() == 1
            && !dynamic_cast<const NamedAssociationExpr*>(aggregate->elements.front().get());
        if (parenthesized)
            return std::move(aggregate->elements.front());
        return aggregate;
    }

    /// A positional value, or `choices => value`.
    ExpressionPtr Parser::parseAggregateElement()
    {
        ExpressionPtr first = parseChoice();
        const bool named = tokens.atAny({ "|", "=>" }) || dynamic_cast<const OthersExpr*>(first.get());
        if (!named)
            return first;
        return finishNamedElement(parseChoicesFrom(std::move(first)));
    }

    ExpressionPtr Parser::finishNamedElement(ExpressionPtr choices)
    {
        auto element = std::make_unique<NamedAssociationExpr>();
        element->source = choices->source;
        element->formal = std::move(choices);
        tokens.expect("=>");
        element->actual = parseExpression();
        return element;
    }

} // namespace Pulse::Parser
