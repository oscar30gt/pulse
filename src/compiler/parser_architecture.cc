#include "parser_internal.h"

namespace Pulse::Parser
{
    std::unique_ptr<ArchitectureDeclaration> ParseContext::parseArch()
    {
        auto arch = std::make_unique<ArchitectureDeclaration>();
        auto* archTok = expect("architecture");
        arch->source = { archTok->line, archTok->column };
        arch->name = expectIdentifier()->value;
        expect("of");
        arch->entityName = expectIdentifier()->value;
        expect("is");

        // Declarations region: signals and components. This continues until we hit the "begin" keyword.
        while (peek() && peek()->value != "begin")
        {
            if (peek() && peek()->value == "signal")
            {
                auto signals = parseSignalDeclaration();
                for (auto& signal : signals)
                    arch->signals.push_back(std::move(signal));
            }
            else if (peek() && peek()->value == "component")
            {
                arch->components.push_back(parseComponentDeclaration());
            }
            else
            {
                error("Unexpected token in architecture declaration: " + peek()->value);
            }
        }

        next(); // "begin" keyword

        // Concurrent region: parse statements until we hit the "end" keyword.
        while (peek() && peek()->value != "end")
        {
            auto* tok = peek();

            // keyowrd-introduced statements
            if (tok->value == "with")
            {
                arch->body.push_back(parseWithClause());
            }
            else if (tok->value == "process") // Unlabeled process statement
            {
                arch->body.push_back(parseProcess("unlabeled_process"));
            }

            // labeled statements & signal assignments
            else if (tok->type == TokenType::Identifier)
            {
                auto* nextTok = peek(1);
                if (!nextTok) error("Unexpected end of file after identifier: " + tok->value);

                if (nextTok->value == ":") // labeled statement (<label>: <statement>) 
                {
                    next(); // consume the label token
                    expect(":");

                    if (peek() && peek()->value == "process")
                    {
                        arch->body.push_back(parseProcess(tok->value));
                    }
                    else
                    {
                        arch->body.push_back(parseComponentInstantiation(tok->value));
                    }
                }

                else // signal assignment
                {
                    arch->body.push_back(parseSignalAssignment());
                }
            }

            else
            {
                error("Unexpected token in architecture body: " + tok->value);
            }
        }

        expect("end");
        maybe("architecture");
        maybe(arch->name);
        expect(";");
        return arch;
    }

    ComponentDeclaration ParseContext::parseComponentDeclaration()
    {
        ComponentDeclaration component;

        auto* componentTok = expect("component");
        component.source = { componentTok->line, componentTok->column };
        component.name = expectIdentifier()->value;
        maybe("is");

        if (peek()->value == "port")
        {
            component.ports = parsePortList();
        }

        expect("end");
        maybe("component");
        maybe(component.name);
        expect(";");
        return component;
    }

    std::vector<SignalDeclaration> ParseContext::parseSignalDeclaration()
    {
        std::vector<SignalDeclaration> signals;
        expect("signal");

        do
        {
            auto* idTok = expectIdentifier();
            SignalDeclaration signal;
            signal.source = { idTok->line, idTok->column };
            signal.name = idTok->value;
            signals.push_back(std::move(signal));
        } while (maybe(","));


        expect(":");
        TypeSpec typeSpec = parseTypeSpec();
        ExpressionPtr initialValue = nullptr;

        if (maybe(":="))
        {
            initialValue = parseExpression();
        }

        // Optimized allocation: clone for all but the last, which can be moved directly.
        for (size_t i = 0; i < signals.size(); ++i)
        {
            bool isLast = (i == signals.size() - 1);

            if (initialValue)
            {
                signals[i].initialValue = isLast
                    ? std::move(initialValue)
                    : cloneExpression(initialValue.get());
            }

            signals[i].typeSpec = isLast
                ? std::move(typeSpec)
                : cloneTypeSpec(typeSpec);
        }

        expect(";");
        return signals;
    }
}