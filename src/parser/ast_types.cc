#include "ast_internal.h"
#include <algorithm>

namespace Pulse::Parser
{
    TypeSpec ParseContext::parseTypeSpec()
    {
        TypeSpec spec;
        auto* typeTok = expectIdentifier();
        spec.source = { typeTok->line, typeTok->column };
        spec.typeName = typeTok->value;

        if (peek()->value != "(")
            return spec; // No args specified, return as is.

        expect("(");
        while (peek()->value != ")")
        {
            spec.args.push_back(parseExpression());
            if (peek()->value == ")") break;
            expect(",");
        }
        
        expect(")");
        return spec;
    }
    
} // namespace Pulse::Parser
