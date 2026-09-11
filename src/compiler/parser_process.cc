#include "parser_internal.h"

namespace Pulse::Parser
{
    // Forward declarations for private helpers
    static std::unique_ptr<SequentialStatement> parseWaitStatementHelper(ParseContext& ctx);

    // Helper: Parse an if/elsif/else statement
    std::unique_ptr<IfStatement> ParseContext::parseIfStatement()
    {
        auto ifStmt = std::make_unique<IfStatement>();
        auto* ifTok = expect("if");
        ifStmt->source = { ifTok->line, ifTok->column };
        
        // Parse if condition and body
        auto condition = parseExpression();
        expect("then");
        
        IfStatement::Branch ifBranch;
        ifBranch.condition = std::move(condition);
        
        // Parse statements until elsif/else/end if
        while (peek() && 
               peek()->value != "elsif" && 
               peek()->value != "else" && 
               peek()->value != "end")
        {
            auto* tok = peek();
            
            if (tok->value == "if")
            {
                ifBranch.body.push_back(parseIfStatement());
            }
            else if (tok->value == "wait")
            {
                ifBranch.body.push_back(parseWaitStatement());
            }
            else if (tok->type == TokenType::Identifier)
            {
                // Signal assignment
                ifBranch.body.push_back(parseSignalAssignment());
            }
            else
            {
                error("Unexpected token in if statement body: " + tok->value);
            }
        }
        
        ifStmt->branches.push_back(std::move(ifBranch));
        
        // Parse elsif clauses
        while (peek() && peek()->value == "elsif")
        {
            next(); // consume "elsif"
            auto elsifCond = parseExpression();
            expect("then");
            
            IfStatement::Branch elsifBranch;
            elsifBranch.condition = std::move(elsifCond);
            
            // Parse elsif body
            while (peek() && 
                   peek()->value != "elsif" && 
                   peek()->value != "else" && 
                   peek()->value != "end")
            {
                auto* tok = peek();
                
                if (tok->value == "if")
                {
                    elsifBranch.body.push_back(parseIfStatement());
                }
                else if (tok->value == "wait")
                {
                    elsifBranch.body.push_back(parseWaitStatement());
                }
                else if (tok->type == TokenType::Identifier)
                {
                    elsifBranch.body.push_back(parseSignalAssignment());
                }
                else
                {
                    error("Unexpected token in elsif statement body: " + tok->value);
                }
            }
            
            ifStmt->branches.push_back(std::move(elsifBranch));
        }
        
        // Parse else clause (optional)
        if (peek() && peek()->value == "else")
        {
            next(); // consume "else"
            
            // Parse else body
            while (peek() && peek()->value != "end")
            {
                auto* tok = peek();
                
                if (tok->value == "if")
                {
                    ifStmt->elseBody.push_back(parseIfStatement());
                }
                else if (tok->value == "wait")
                {
                    ifStmt->elseBody.push_back(parseWaitStatement());
                }
                else if (tok->type == TokenType::Identifier)
                {
                    ifStmt->elseBody.push_back(parseSignalAssignment());
                }
                else
                {
                    error("Unexpected token in else statement body: " + tok->value);
                }
            }
        }
        
        expect("end");
        expect("if");
        expect(";");
        
        return ifStmt;
    }

    // Helper: Parse a wait statement
    std::unique_ptr<SequentialStatement> ParseContext::parseWaitStatement()
    {
        auto* waitTok = expect("wait");
        
        // Check for "wait for <time>" or just "wait;"
        if (peek() && peek()->value == "for")
        {
            next(); // consume "for"
            
            auto* timeTok = peek();
            if (!timeTok || timeTok->type != TokenType::NumericLiteral)
            {
                error("Expected numeric literal for wait duration");
            }
            next();
            
            uint64_t duration = std::stoull(timeTok->value);
            
            // Parse time unit (fs, ps, ns, us, ms) and convert to femtoseconds
            if (peek() && peek()->type == TokenType::Identifier)
            {
                auto* unitTok = peek();
                std::string unit = unitTok->value;
                next();
                
                if (unit == "fs")       duration *= 1;
                else if (unit == "ps")  duration *= 1000;
                else if (unit == "ns")  duration *= 1000000;
                else if (unit == "us")  duration *= 1000000000;
                else if (unit == "ms")  duration *= 1000000000000;
                else error("Unknown time unit: " + unit);
            }
            
            auto waitStmt = std::make_unique<WaitForStatement>();
            waitStmt->source = { waitTok->line, waitTok->column };
            waitStmt->durationFs = duration;
            expect(";");
            return waitStmt;
        }
        else
        {
            // Just "wait;" - treated as wait forever
            // Return nullptr for WaitForStatement, which indicates wait forever
            expect(";");
            auto waitStmt = std::make_unique<WaitForeverStatement>();
            waitStmt->source = { waitTok->line, waitTok->column };
            return waitStmt;
        }
    }

    std::unique_ptr<ProcessStatement> ParseContext::parseProcess(const std::string& processLabel)
    {
        auto process = std::make_unique<ProcessStatement>();
        auto* processTok = expect("process");
        process->source = { processTok->line, processTok->column };
        process->label = processLabel;
        
        // Parse optional sensitivity list
        if (peek() && peek()->value == "(")
        {
            next(); // consume (
            
            if (peek()->value != ")")
            {
                do
                {
                    auto* idTok = expectIdentifier();
                    process->sensitivityList.push_back(idTok->value);
                } while (maybe(","));
            }
            
            expect(")");
        }
        
        expect("begin");
        
        // Parse sequential statements until "end process"
        while (peek() && peek()->value != "end")
        {
            auto* tok = peek();
            
            if (tok->value == "if")
            {
                process->body.push_back(parseIfStatement());
            }
            else if (tok->value == "wait")
            {
                process->body.push_back(parseWaitStatement());
            }
            else if (tok->type == TokenType::Identifier)
            {
                // Signal assignment
                process->body.push_back(parseSignalAssignment());
            }
            else
            {
                error("Unexpected token in process body: " + tok->value);
            }
        }
        
        expect("end");
        expect("process");
        maybe(processLabel); // Optional label after end process
        expect(";");
        
        return process;
    }

} // namespace Pulse::Parser
