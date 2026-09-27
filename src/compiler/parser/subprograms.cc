#include "parser_impl.h"

namespace Pulse::Parser
{
    /// `spec ;` (a declaration) or `spec is ... end ;` (a body).
    DeclarationPtr Parser::parseSubprogram()
    {
        std::unique_ptr<SubprogramSpec> spec = parseSubprogramSpec();
        if (tokens.at("is"))
            return parseSubprogramBody(std::move(spec));

        auto declaration = std::make_unique<SubprogramDeclaration>();
        declaration->source = spec->source;
        declaration->spec = std::move(spec);
        tokens.expect(";");
        return declaration;
    }

    // ---- Specifications -------------------------------------------------------------------------

    std::unique_ptr<SubprogramSpec> Parser::parseSubprogramSpec()
    {
        const Token* start = tokens.peek();
        if (tokens.at("procedure"))
            return parseProcedureSpec();

        const bool impure = tokens.accept("impure") != nullptr;
        if (!impure)
            tokens.accept("pure");
        return parseFunctionSpec(start, impure);
    }

    /// `function name [(parameters)] return type_mark`, after `pure` / `impure`.
    std::unique_ptr<SubprogramSpec> Parser::parseFunctionSpec(const Token* start, bool impure)
    {
        auto spec = node<SubprogramSpec>(start);
        tokens.expect("function");
        spec->kind = SubprogramKind::Function;
        spec->impure = impure;
        spec->name = parseSubprogramName();
        spec->parameters = parseOptionalParameters();
        tokens.expect("return");
        spec->returnType = parseTypeMark();
        return spec;
    }

    /// `procedure name [(parameters)]`
    std::unique_ptr<SubprogramSpec> Parser::parseProcedureSpec()
    {
        auto spec = node<SubprogramSpec>(tokens.expect("procedure"));
        spec->kind = SubprogramKind::Procedure;
        spec->name = parseSubprogramName();
        spec->parameters = parseOptionalParameters();
        return spec;
    }

    /// An identifier, or an operator symbol such as `"and"` (overloaded operators).
    std::string Parser::parseSubprogramName()
    {
        if (tokens.atType(TokenType::StringLiteral))
            return operatorSymbolName(*tokens.next());
        return tokens.expectIdentifier()->value;
    }

    /// `[parameter] ( parameters )`, or nothing.
    std::vector<std::unique_ptr<ParameterDeclaration>> Parser::parseOptionalParameters()
    {
        if (tokens.accept("parameter"))
            return parseParameterList();
        if (tokens.at("("))
            return parseParameterList();
        return {};
    }

    // ---- Bodies ---------------------------------------------------------------------------------

    /// `is declarations begin statements end [procedure|function] [name] ;`
    std::unique_ptr<SubprogramBody> Parser::parseSubprogramBody(std::unique_ptr<SubprogramSpec> spec)
    {
        NestingGuard guard(*this);
        auto body = std::make_unique<SubprogramBody>();
        body->source = spec->source;
        tokens.expect("is");
        body->declarations = parseDeclarativePart(Region::Subprogram);
        tokens.expect("begin");
        body->body = parseSequentialStatements({ "end" });
        expectEndOptionalKind(spec->kind == SubprogramKind::Function ? "function" : "procedure", spec->name);
        body->spec = std::move(spec);
        return body;
    }

} // namespace Pulse::Parser
