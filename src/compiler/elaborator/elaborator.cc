#include "elaborator_internal.h"

namespace Pulse::Parser
{
    // ---- Entry point ----------------------------------------------------------------------------

    ElaboratedDesign elaborate(const DesignLibrary& library, const ASTRoot& linkedDesign, const ElaborationOptions& options)
    {
        DesignElaborator design(library, options);
        return design.run(linkedDesign);
    }

    std::string describeValue(const StaticValue& value)
    {
        switch (value.kind)
        {
            case StaticValue::Kind::Integer:
                return std::to_string(value.integer);
            case StaticValue::Kind::Real:
                return std::to_string(value.real);
            case StaticValue::Kind::Physical:
                return std::to_string(value.integer) + " (base units)";
            case StaticValue::Kind::Enumeration:
                if (value.type && value.integer >= 0 && static_cast<size_t>(value.integer) < value.type->literals.size())
                    return value.type->literals[static_cast<size_t>(value.integer)];
                return "position " + std::to_string(value.integer);
            case StaticValue::Kind::Vector:
                return "\"" + value.bits.str(value.width) + "\"";
        }
        return "?";
    }

    // ---- Design ---------------------------------------------------------------------------------

    DesignElaborator::DesignElaborator(const DesignLibrary& library, const ElaborationOptions& options)
        : m_library(library), m_options(options)
    { }

    ElaboratedDesign DesignElaborator::run(const ASTRoot& linkedDesign)
    {
        const SourceLocation start{ 1, 1 };

        if (m_options.logic != LogicMode::Logic)
            throw elaboration_error("Nine-valued std_logic vectors are not supported yet: their operators need the bodies of the IEEE "
                                    "functions, which are not implemented. Elaborate with -Ologic", start);

        const EntityDeclaration* top = nullptr;
        for (const auto& unit : linkedDesign.children)
            if (auto* entity = dynamic_cast<const EntityDeclaration*>(unit.get()); entity && entity->name == m_options.topEntity)
                top = entity;

        if (!top || m_library.entity(top->name) != top)
            throw elaboration_error("Top-level entity '" + m_options.topEntity + "' not found in the design; give the name of the top "
                                    "entity (--top)", start);

        const ArchitectureDeclaration* architecture = m_options.topArchitecture.empty()
            ? m_library.latestArchitecture(top->name)
            : m_library.architecture(top->name, m_options.topArchitecture);

        if (!architecture)
            throw elaboration_error(m_options.topArchitecture.empty()
                ? "Entity '" + top->name + "' has no architecture to elaborate"
                : "Entity '" + top->name + "' has no architecture '" + m_options.topArchitecture + "'", top->source);

        m_design.top = specialize(*top, *architecture, {}, *top).blueprint;
        return std::move(m_design);
    }

    const Specialization& DesignElaborator::specialize(const EntityDeclaration& entity, const ArchitectureDeclaration& architecture,
                                                       const GenericValues& generics, const ASTNode& at)
    {
        for (const std::string& building : m_building)
            if (building == entity.name)
                throw elaboration_error("Entity '" + entity.name + "' instantiates itself, directly or through other entities; the "
                                        "hierarchy would never end", at.source);

        auto blueprint = std::make_unique<Engine::Blueprint>();
        auto specialization = std::make_unique<Specialization>();
        specialization->blueprint = blueprint.get();
        specialization->entityName = entity.name;

        UnitElaborator unit(*this, entity, architecture, generics, *blueprint, *specialization);
        const GenericValues complete = unit.completeGenerics(at);

        // Like a C++ template: one blueprint per distinct set of generic values.
        std::string key = entity.name + "(" + architecture.name + ")";
        for (const auto& [generic, value] : complete)
            key += ";" + generic->name + "=" + describeValue(value);

        if (auto found = m_specializations.find(key); found != m_specializations.end())
            return *found->second;

        m_building.push_back(entity.name);
        unit.run();
        m_building.pop_back();

        m_design.blueprints.push_back(std::move(blueprint));
        auto& stored = m_specializations[key];
        stored = std::move(specialization);
        return *stored;
    }

    // ---- One architecture -----------------------------------------------------------------------

    UnitElaborator::UnitElaborator(DesignElaborator& design, const EntityDeclaration& entity, const ArchitectureDeclaration& architecture,
                                   const GenericValues& generics, Engine::Blueprint& blueprint, Specialization& specialization)
        : m_design(design), m_library(design.library()), m_entity(entity), m_architecture(architecture), m_bp(blueprint),
          m_spec(specialization)
    {
        for (const auto& [generic, value] : generics)
            m_values[generic] = value;
    }

    void UnitElaborator::fail(const std::string& message, const ASTNode& at) const
    {
        throw elaboration_error(message, at.source);
    }

    void UnitElaborator::unsupported(const std::string& what, const ASTNode& at) const
    {
        throw elaboration_error(what + " is not supported yet", at.source);
    }

    GenericValues UnitElaborator::completeGenerics(const ASTNode& at)
    {
        GenericValues complete;
        for (const auto& generic : m_entity.generics)
        {
            auto given = m_values.find(generic.get());
            if (given == m_values.end())
            {
                if (!generic->defaultValue)
                    fail("Generic '" + generic->name + "' of entity '" + m_entity.name + "' has no value: give it a default value, or a "
                         "value in the generic map of the instance", at);
                given = m_values.emplace(generic.get(), requireStatic(*generic->defaultValue, "The default value of generic '"
                                                                      + generic->name + "'")).first;
            }
            complete.emplace_back(generic.get(), given->second);
        }
        return complete;
    }

    void UnitElaborator::run()
    {
        m_typeScopes.emplace_back();

        declarePorts();
        elaborateDeclarations(m_architecture.declarations, false);

        for (const auto& statement : m_architecture.body)
            elaborateStatement(*statement);

        checkDrivers();
        fillSymbols();
    }

    void UnitElaborator::declarePorts()
    {
        for (const auto& port : m_entity.ports)
        {
            const SemanticType* type = m_library.interfaceType(*port);
            if (!type)
                fail("Port '" + port->name + "' of entity '" + m_entity.name + "' was not analyzed", *port);

            const Layout layout = layoutOf(*type, port->typeSpec.get(), *port);
            const LogicVector initial = port->defaultValue
                ? bitsOf(requireStatic(*port->defaultValue, "The default value of port '" + port->name + "'", &layout), layout, *port->defaultValue)
                : defaultValue(layout, *type);

            m_bp.addPort(port->name, port->mode == PortMode::In, Engine::WireInstance{ layout.width, initial });

            ObjectWire object;
            object.objectClass = ObjectWire::Class::Port;
            object.name = port->name;
            object.wire = port->name;
            object.layout = layout;
            object.mode = port->mode;
            object.declaration = port.get();
            object.defaultValue = initial;
            m_objects[port.get()] = object;

            m_spec.ports[port->name] = layout;
            m_spec.portDefaults[port->name] = initial;
        }
    }

    // ---- Declarations ---------------------------------------------------------------------------

    void UnitElaborator::elaborateDeclarations(const std::vector<DeclarationPtr>& declarations, bool inProcess)
    {
        for (const auto& declaration : declarations)
        {
            const Declaration& decl = *declaration;

            if (auto* signal = dynamic_cast<const SignalDeclaration*>(&decl))
                declareSignal(*signal);
            else if (auto* constant = dynamic_cast<const ConstantDeclaration*>(&decl))
                declareConstant(*constant);
            else if (auto* variable = dynamic_cast<const VariableDeclaration*>(&decl); variable && inProcess)
                variableWire(*variable);
            else if (auto* type = dynamic_cast<const TypeDeclaration*>(&decl))
                m_typeScopes.back()[type->name] = type;
            else if (auto* subtype = dynamic_cast<const SubtypeDeclaration*>(&decl))
                m_typeScopes.back()[subtype->name] = subtype;
            else if (dynamic_cast<const AliasDeclaration*>(&decl))
                unsupported("Aliases", decl);
            // Components, subprograms (their calls are reported), attributes and context clauses need nothing here.
        }
    }

    void UnitElaborator::declareSignal(const SignalDeclaration& decl)
    {
        const SemanticType* type = m_library.objectType(decl);
        if (!type)
            fail("Signal '" + decl.name + "' was not analyzed", decl);

        const Layout layout = layoutOf(*type, decl.typeSpec.get(), decl);
        const LogicVector initial = decl.initialValue
            ? bitsOf(requireStatic(*decl.initialValue, "The initial value of signal '" + decl.name + "'", &layout), layout, *decl.initialValue)
            : defaultValue(layout, *type);

        m_bp.addSignal(decl.name, layout.width, initial);

        ObjectWire object;
        object.objectClass = ObjectWire::Class::Signal;
        object.name = decl.name;
        object.wire = decl.name;
        object.layout = layout;
        object.declaration = &decl;
        object.defaultValue = initial;
        m_objects[&decl] = object;
    }

    void UnitElaborator::declareConstant(const ConstantDeclaration& decl)
    {
        // A constant of a constrained vector type has the length of its type; an unconstrained one takes its value's.
        const SemanticType* type = m_library.objectType(decl);
        std::optional<Layout> layout;
        if (type && isArray(*type) && (isConstrainedArray(*type) || !decl.typeSpec->args.empty()))
            layout = layoutOf(*type, decl.typeSpec.get(), decl);

        const StaticValue value = requireStatic(*decl.value, "The value of constant '" + decl.name + "'", layout ? &*layout : nullptr);
        if (layout && value.kind == StaticValue::Kind::Vector && layout->width != value.width)
            fail("Constant '" + decl.name + "' has " + std::to_string(layout->width) + " elements but its value has "
                 + std::to_string(value.width), decl);
        m_values[&decl] = value;
    }

    // ---- Concurrent statements ------------------------------------------------------------------

    void UnitElaborator::elaborateStatement(const Statement& statement)
    {
        if (auto* assignment = dynamic_cast<const SignalAssignment*>(&statement))
            elaborateSignalAssignment(*assignment);
        else if (auto* with = dynamic_cast<const WithClause*>(&statement))
            elaborateWithClause(*with);
        else if (auto* instance = dynamic_cast<const ComponentInstantiation*>(&statement))
            elaborateInstance(*instance);
        else if (auto* process = dynamic_cast<const ProcessStatement*>(&statement))
            elaborateProcess(*process);
        else if (dynamic_cast<const AssertStatement*>(&statement))
            unsupported("The assert statement", statement);
        else if (dynamic_cast<const ProcedureCallStatement*>(&statement))
            unsupported("Calling a procedure", statement);
        else
            unsupported("This concurrent statement", statement);
    }

} // namespace Pulse::Parser
