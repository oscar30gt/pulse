#include "analyzer_internal.h"

namespace Pulse::Parser
{
    void AnalyzerContext::analyzeDeclarations(const std::vector<DeclarationPtr>& declarations)
    {
        for (const auto& declaration : declarations)
        {
            const auto* handler = m_declarations.find(*declaration);
            if (!handler)
                fail("The analyzer has no handler for this kind of declaration", *declaration);

            (*handler)(*declaration);
        }

        checkBodiesDefined();
    }

    /// An initial value is checked against the declared type and must not read signals or variables.
    void AnalyzerContext::checkInitialValue(const Expression& init, const SemanticType& target, const std::string& what)
    {
        if (!isStaticExpression(init))
            fail("The initial value of " + what + " must be constant: it cannot read signals, ports or variables", init);

        const SemanticType valueType = exprType(init, &target);
        checkAssignable(target, valueType, &init, init, what);
    }

    void AnalyzerContext::declareLibraryClause(const LibraryClause&) { }

    void AnalyzerContext::declareUseClause(const UseClause&) { }

    // ---- Objects --------------------------------------------------------------------------------

    void AnalyzerContext::declareSignal(const SignalDeclaration& decl)
    {
        Symbol symbol;
        symbol.kind = SymbolKind::Signal;
        symbol.type = resolveObjectType(*decl.typeSpec, "Signal", decl.name);
        symbol.objectId = newObjectId();

        if (decl.initialValue)
            checkInitialValue(*decl.initialValue, symbol.type, "signal '" + decl.name + "'");

        declare(decl.name, std::move(symbol), decl);
    }

    void AnalyzerContext::declareVariable(const VariableDeclaration& decl)
    {
        Symbol symbol;
        symbol.kind = SymbolKind::Variable;
        symbol.type = resolveObjectType(*decl.typeSpec, "Variable", decl.name);
        symbol.objectId = newObjectId();

        if (decl.initialValue)
            checkInitialValue(*decl.initialValue, symbol.type, "variable '" + decl.name + "'");

        declare(decl.name, std::move(symbol), decl);
    }

    void AnalyzerContext::declareConstant(const ConstantDeclaration& decl)
    {
        Symbol symbol;
        symbol.kind = SymbolKind::Constant;
        symbol.type = resolveTypeSpec(*decl.typeSpec);
        symbol.objectId = newObjectId();

        const std::string what = "constant '" + decl.name + "'";
        if (!isStaticExpression(*decl.value))
            fail("The value of " + what + " must be constant: it cannot read signals, ports or variables", *decl.value);

        const SemanticType valueType = exprType(*decl.value, &symbol.type);

        // `constant c : std_logic_vector := "0101";` takes its index range from the value.
        if (isArray(symbol.type) && !isConstrainedArray(symbol.type))
        {
            if (valueType.info != symbol.type.info || (valueType.dims.empty() && !valueType.unknownBounds))
                fail("Cannot determine the index range of " + what + "; give its type an explicit range", decl);
            symbol.type.dims = valueType.dims;
            symbol.type.unknownBounds = valueType.unknownBounds;
        }

        checkAssignable(symbol.type, valueType, decl.value.get(), *decl.value, what);
        symbol.value = fold(*decl.value, &symbol.type);
        declare(decl.name, std::move(symbol), decl);
    }

    // ---- Components -----------------------------------------------------------------------------

    void AnalyzerContext::declareComponent(const ComponentDeclaration& decl)
    {
        auto entity = m_entityInterfaces.find(decl.name);
        if (entity == m_entityInterfaces.end())
            fail("Component '" + decl.name + "' has no entity of the same name", decl);

        // The component's own generics are visible in its ports, so they are resolved in a scope of their own.
        pushScope();
        std::vector<FormalInfo> generics = resolveGenerics(decl.generics);
        std::vector<FormalInfo> ports = resolvePorts(decl.ports, "component '" + decl.name + "'");
        popScope();

        checkComponentAgainstEntity(decl, entity->second, generics, ports);

        Symbol symbol;
        symbol.kind = SymbolKind::Component;
        symbol.component = &decl;
        symbol.componentGenerics = std::move(generics);
        symbol.componentPorts = std::move(ports);
        declare(decl.name, std::move(symbol), decl);
    }

} // namespace Pulse::Parser
