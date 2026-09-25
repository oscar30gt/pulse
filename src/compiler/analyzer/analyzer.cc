#include "analyzer_internal.h"

namespace Pulse::Parser
{
    AnalyzerContext::AnalyzerContext()
    {
        registerDispatchTables();
        registerExpressionHandlers();
        registerSequentialHandlers();
    }

    /// Supporting a new kind of type definition, declaration or concurrent statement means adding one line here.
    void AnalyzerContext::registerDispatchTables()
    {
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

    // ---- Entities -------------------------------------------------------------------------------

    void AnalyzerContext::collectEntities(const ASTRoot& root)
    {
        m_entities.clear();
        m_architectureNames.clear();

        for (const auto& child : root.children)
        {
            if (auto* architecture = dynamic_cast<const ArchitectureDeclaration*>(child.get()))
            {
                m_architectureNames.insert(architecture->name);
                continue;
            }

            auto* entity = dynamic_cast<const EntityDeclaration*>(child.get());
            if (!entity)
                fail("The analyzer has no handler for this kind of design unit", *child);

            if (!m_entities.emplace(entity->name, entity).second)
                fail("Entity '" + entity->name + "' is declared twice", *entity);
        }
    }

    /// Generics and ports are resolved once, in a scope that only sees the predefined types (and the generics).
    void AnalyzerContext::analyzeEntity(const EntityDeclaration& entity)
    {
        pushScope();
        analyzeDeclarations(entity.context);

        EntityInterface interface;
        interface.generics = resolveGenerics(entity.generics);
        interface.ports = resolvePorts(entity.ports, "entity '" + entity.name + "'");
        popScope();

        m_entityInterfaces[entity.name] = std::move(interface);
    }

    // ---- Architectures --------------------------------------------------------------------------

    void AnalyzerContext::analyzeArchitecture(const ArchitectureDeclaration& arch)
    {
        auto entity = m_entities.find(arch.entityName);
        if (entity == m_entities.end())
            fail("Architecture '" + arch.name + "' belongs to the unknown entity '" + arch.entityName + "'", arch);

        m_drivers.clear();
        m_pendingLabelSpecs.clear();
        m_statementCalls.clear();
        const size_t firstSubprogram = m_subprograms.size();

        pushScope();
        analyzeDeclarations(arch.context);

        const EntityInterface& entityInterface = m_entityInterfaces.at(arch.entityName);
        declareGenericSymbols(entityInterface.generics);
        declarePortSymbols(entityInterface.ports);
        analyzeDeclarations(arch.declarations);

        for (const auto& statement : arch.body)
            analyzeConcurrentStatement(*statement);

        resolvePendingLabelSpecs();
        checkCallGraph(firstSubprogram);
        checkDrivers();
        popScope();
    }

    // ---- Entry point ----------------------------------------------------------------------------

    void AnalyzerContext::analyze(const ASTRoot& root)
    {
        loadPrelude();
        collectEntities(root);

        for (const auto& child : root.children)
            if (auto* entity = dynamic_cast<const EntityDeclaration*>(child.get()))
                analyzeEntity(*entity);

        for (const auto& child : root.children)
            if (auto* arch = dynamic_cast<const ArchitectureDeclaration*>(child.get()))
                analyzeArchitecture(*arch);
    }

    void analyzeAST(const ASTRoot& root)
    {
        AnalyzerContext context;
        context.analyze(root);
    }

} // namespace Pulse::Parser
