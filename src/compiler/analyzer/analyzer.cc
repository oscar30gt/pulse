#include "analyzer_internal.h"

namespace Pulse::Parser
{
    AnalyzerContext::AnalyzerContext()
    {
        registerDispatchTables();
        registerExpressionHandlers();
        registerSequentialHandlers();
        loadPrelude();
    }

    /// Supporting a new kind of design unit, type definition, declaration or concurrent statement means adding one line here.
    void AnalyzerContext::registerDispatchTables()
    {
        m_units.add(*this, &AnalyzerContext::analyzeEntity);
        m_units.add(*this, &AnalyzerContext::analyzeArchitecture);

        m_definitions.add(*this, &AnalyzerContext::defineNumeric);
        m_definitions.add(*this, &AnalyzerContext::defineEnumeration);
        m_definitions.add(*this, &AnalyzerContext::definePhysical);
        m_definitions.add(*this, &AnalyzerContext::defineConstrainedArray);
        m_definitions.add(*this, &AnalyzerContext::defineUnconstrainedArray);
        m_definitions.add(*this, &AnalyzerContext::defineRecord);

        m_declarations.add(*this, &AnalyzerContext::declareType);
        m_declarations.add(*this, &AnalyzerContext::declareSubtype);
        m_declarations.add(*this, &AnalyzerContext::declareSignal);
        m_declarations.add(*this, &AnalyzerContext::declareConstant);
        m_declarations.add(*this, &AnalyzerContext::declareVariable);
        m_declarations.add(*this, &AnalyzerContext::declareComponent);
        m_declarations.add(*this, &AnalyzerContext::declareLibraryClause);
        m_declarations.add(*this, &AnalyzerContext::declareUseClause);
        m_declarations.add(*this, &AnalyzerContext::declareSubprogram);
        m_declarations.add(*this, &AnalyzerContext::declareSubprogramBody);
        m_declarations.add(*this, &AnalyzerContext::declareAlias);
        m_declarations.add(*this, &AnalyzerContext::declareAttribute);
        m_declarations.add(*this, &AnalyzerContext::specifyAttribute);
        m_declarations.add(*this, &AnalyzerContext::misplacedDeclaration<GenericDeclaration>);
        m_declarations.add(*this, &AnalyzerContext::misplacedDeclaration<PortDeclaration>);
        m_declarations.add(*this, &AnalyzerContext::misplacedDeclaration<ParameterDeclaration>);

        m_concurrent.add(*this, &AnalyzerContext::analyzeSignalAssignment);
        m_concurrent.add(*this, &AnalyzerContext::analyzeWithClause);
        m_concurrent.add(*this, &AnalyzerContext::analyzeComponentInstantiation);
        m_concurrent.add(*this, &AnalyzerContext::analyzeProcess);
        m_concurrent.add(*this, &AnalyzerContext::analyzeAssert);
        m_concurrent.add(*this, &AnalyzerContext::analyzeProcedureCall);
    }

    /// Nothing a design unit declares outlives it except what joins the library, so the next unit starts from the prelude alone.
    void AnalyzerContext::beginUnit()
    {
        m_scopes.resize(1);
        m_architecture = nullptr;
        m_architectureAttributes.clear();
        m_loops.clear();
        m_process = nullptr;
        m_subprogram = nullptr;
        m_bodyInProcess = false;
        m_bodyDepth = 0;
        m_drivers.clear();
        m_driverSource = nullptr;
        m_driverDescription.clear();
        m_pendingLabelSpecs.clear();
        m_statementCalls.clear();
        m_pendingBodies.clear();
    }

    // ---- Entities -------------------------------------------------------------------------------

    /// Generics and ports are resolved once, in a scope that only sees the predefined types (and the generics). The entity
    /// joins the library only when they are.
    void AnalyzerContext::analyzeEntity(const EntityDeclaration& entity)
    {
        if (m_entities.count(entity.name))
            fail("Entity '" + entity.name + "' is declared twice", entity);

        pushScope();
        analyzeDeclarations(entity.context);

        LibraryEntity unit;
        unit.declaration = &entity;
        unit.generics = resolveGenerics(entity.generics);
        unit.ports = resolvePorts(entity.ports, "entity '" + entity.name + "'");
        popScope();

        m_entities.emplace(entity.name, std::move(unit));
    }

    // ---- Architectures --------------------------------------------------------------------------

    /// An architecture is analyzed against the entity the library holds for it (LRM 13.5), so the entity may come from an
    /// earlier file; in the same file it must come first, since units are analyzed in textual order.
    void AnalyzerContext::analyzeArchitecture(const ArchitectureDeclaration& arch)
    {
        auto found = m_entities.find(arch.entityName);
        if (found == m_entities.end())
        {
            if (m_fileEntities.count(arch.entityName))
                fail("Architecture '" + arch.name + "' comes before its entity '" + arch.entityName + "'; an entity must be analyzed "
                     "before its architectures, so declare it first", arch);
            fail("Architecture '" + arch.name + "' belongs to the unknown entity '" + arch.entityName + "'", arch);
        }

        LibraryEntity& entity = found->second;
        if (entity.architectures.count(arch.name))
            fail("Architecture '" + arch.name + "' of entity '" + arch.entityName + "' is declared twice", arch);

        m_architecture = &arch;
        const size_t firstSubprogram = m_subprograms.size();

        pushScope();
        analyzeDeclarations(arch.context);

        declareGenericSymbols(entity.generics);
        declarePortSymbols(entity.ports);
        analyzeDeclarations(arch.declarations);

        for (const auto& statement : arch.body)
            analyzeConcurrentStatement(*statement);

        resolvePendingLabelSpecs();
        checkCallGraph(firstSubprogram);
        checkDrivers();
        popScope();

        m_architecture = nullptr;
        entity.architectures.insert(arch.name);
        entity.architectureOrder.push_back(&arch);
    }

    // ---- Entry point ----------------------------------------------------------------------------

    /// The design units of a file are analyzed in textual order (LRM 13.1), each against the units already in the library.
    void AnalyzerContext::analyze(const ASTRoot& root)
    {
        m_fileEntities.clear();
        for (const auto& unit : root.children)
            if (auto* entity = dynamic_cast<const EntityDeclaration*>(unit.get()))
                m_fileEntities.insert(entity->name);

        for (const auto& unit : root.children)
        {
            if (!unit)
                continue;

            const auto* handler = m_units.find(*unit);
            if (!handler)
                fail("The analyzer has no handler for this kind of design unit", *unit);

            beginUnit();
            (*handler)(*unit);
        }
    }

    // ---- What the linker reads ------------------------------------------------------------------

    const ComponentDeclaration* AnalyzerContext::componentOf(const ComponentInstantiation& instance) const
    {
        auto found = m_instanceComponents.find(&instance);
        return found == m_instanceComponents.end() ? nullptr : found->second;
    }

    const SemanticType* AnalyzerContext::interfaceType(const Declaration& genericOrPort) const
    {
        auto found = m_interfaceTypes.find(&genericOrPort);
        return found == m_interfaceTypes.end() ? nullptr : &found->second;
    }

    // ---- What the elaborator reads --------------------------------------------------------------

    const SemanticType* AnalyzerContext::recordedType(const Expression& expr) const
    {
        auto found = m_expressionTypes.find(&expr);
        return found == m_expressionTypes.end() ? nullptr : &found->second;
    }

    const ASTNode* AnalyzerContext::declarationOf(const SymbolExpr& name) const
    {
        auto found = m_denotations.find(&name);
        return found == m_denotations.end() ? nullptr : found->second;
    }

    std::optional<CallTarget> AnalyzerContext::calleeOf(const ASTNode& callOrOperator) const
    {
        auto found = m_resolvedCalls.find(&callOrOperator);
        if (found == m_resolvedCalls.end())
            return std::nullopt;

        const SubprogramInfo& info = *found->second;
        CallTarget target;
        target.name = info.name;
        target.builtin = info.builtin;
        target.declaration = info.declaration;
        for (const FormalInfo& parameter : info.parameters)
            target.parameters.push_back(parameter.name);
        return target;
    }

    const SemanticType* AnalyzerContext::objectType(const Declaration& decl) const
    {
        auto found = m_objectTypes.find(&decl);
        return found == m_objectTypes.end() ? nullptr : &found->second;
    }

    const EntityDeclaration* AnalyzerContext::entity(const std::string& name) const
    {
        auto found = m_entities.find(name);
        return found == m_entities.end() ? nullptr : found->second.declaration;
    }

    const ArchitectureDeclaration* AnalyzerContext::architecture(const std::string& entityName, const std::string& name) const
    {
        auto found = m_entities.find(entityName);
        if (found == m_entities.end())
            return nullptr;

        for (const ArchitectureDeclaration* arch : found->second.architectureOrder)
            if (arch->name == name)
                return arch;
        return nullptr;
    }

    const TypeInfo* AnalyzerContext::predefinedType(const std::string& name) const
    {
        auto found = m_scopes.front().symbols.find(name);
        if (found == m_scopes.front().symbols.end() || found->second.kind != SymbolKind::Type)
            return nullptr;
        return found->second.type.info;
    }

    const ArchitectureDeclaration* AnalyzerContext::latestArchitecture(const std::string& entityName) const
    {
        auto found = m_entities.find(entityName);
        if (found == m_entities.end() || found->second.architectureOrder.empty())
            return nullptr;
        return found->second.architectureOrder.back();
    }

} // namespace Pulse::Parser
