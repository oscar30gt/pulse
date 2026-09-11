#include "parser_internal.h"

namespace Pulse::Parser
{
    std::unique_ptr<WhenElseExpr> ParseContext::parseWhenElse()
    {
        auto rootExpr = std::make_unique<WhenElseExpr>();
        auto* whenTok = expect("when");
        rootExpr->source = { whenTok->line, whenTok->column };
        
        // Parse the condition
        rootExpr->condition = parseExpression(false);
        expect("else");
        
        // Parse the true value
        rootExpr->trueValue = parseExpression(false);
        
        WhenElseExpr* current = rootExpr.get();
        
        // Parse remaining "when <condition> else <value>" chains recursively building the tree
        while (peek() && peek()->value == "when")
        {
            next(); // consume "when"
            auto cond = parseExpression(false);
            expect("else");
            auto val = parseExpression(false);
            
            auto nextBranch = std::make_unique<WhenElseExpr>();
            nextBranch->source = current->trueValue->source; // Approximate source location
            nextBranch->condition = std::move(cond);
            nextBranch->trueValue = std::move(val);
            
            current->falseValue = std::move(nextBranch);
            current = static_cast<WhenElseExpr*>(current->falseValue.get());
        }
        
        // Default value is optional; if there's no final else, it's nullptr
        current->falseValue = nullptr;
        
        return rootExpr;
    }

    std::unique_ptr<WithClause> ParseContext::parseWithClause()
    {
        auto withClause = std::make_unique<WithClause>();
        auto* withTok = expect("with");
        withClause->source = { withTok->line, withTok->column };
        
        // Parse selector expression
        withClause->selector = parseExpression(false);
        
        expect("select");
        
        // Parse choices: value when condition , value when condition , ... value when others;
        bool isFirst = true;
        while (true)
        {
            if (!isFirst)
            {
                expect({",", ";"});
            }
            isFirst = false;
            
            // Parse value
            auto value = parseExpression(false);
            expect("when");
            
            // Check for "others" keyword for default case
            if (peek() && peek()->value == "others")
            {
                next(); // consume "others"
                withClause->defaultValue = std::move(value);
                expect(";");
                break;
            }
            else
            {
                // Parse choice expression
                auto condition = parseExpression(false);
                withClause->choices.push_back({ std::move(condition), std::move(value) });
                
                // Check if this is the end
                if (peek() && peek()->value == ";")
                {
                    next(); // consume ";"
                    break;
                }
            }
        }
        
        return withClause;
    }

} // namespace Pulse::Parser