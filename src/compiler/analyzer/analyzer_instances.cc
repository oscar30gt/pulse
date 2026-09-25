#include "analyzer_internal.h"

namespace Pulse::Parser
{
    // ---- Selected signal assignment -------------------------------------------------------------

    void AnalyzerContext::analyzeWithClause(const WithClause& with)
    {
        if (m_subprogram)
        {
            if (m_subprogram->isFunction)
                fail("A function cannot assign signals", with);
            m_subprogram->assignsSignals = true;
        }

        if (dynamic_cast<const AggregateExpr*>(with.target.get()))
            fail("An aggregate target is not supported in a selected signal assignment; assign the elements separately", *with.target);

        const SemanticType selector = exprType(*with.selector);
        const SemanticType target = exprType(*with.target);
        checkWritable(*with.target, "a selected signal assignment");
        recordDriver(*with.target, with);

        const std::string what = "signal '" + rootObject(*with.target).name + "'";
        std::vector<const ChoiceListExpr*> lists;

        for (const auto& choice : with.choices)
        {
            analyzeValue(*choice->value, target, with, what, true);
            lists.push_back(&requireChoiceList(*choice->choices));
        }

        analyzeChoiceLists(lists, selector, *with.selector, with.matching ? "the 'with ... select?' statement" : "the 'with' statement",
                           with.matching);
    }

    // ---- Component instantiation ----------------------------------------------------------------

    void AnalyzerContext::analyzeComponentInstantiation(const ComponentInstantiation& instance)
    {
        const Symbol* component = find(instance.componentName);
        if (!component || component->kind != SymbolKind::Component)
            fail("Unknown component '" + instance.componentName + "'; declare it in the architecture before instantiating it", instance);

        checkGenericMap(instance, *component);
        checkPortMap(instance, *component);
    }

    void AnalyzerContext::checkGenericMap(const ComponentInstantiation& instance, const Symbol& component)
    {
        const auto& generics = component.componentGenerics;
        const std::string owner = "instance '" + instance.label + "'";

        AssociationResult result = matchAssociations(generics, associationsOf(instance.genericMap), AssociationKind::GenericMap, owner, instance);
        if (!result.ok())
            fail(result.problem, *result.at);

        for (const Binding& binding : result.bindings)
            checkBinding(binding, generics[binding.formal], AssociationKind::GenericMap, "generic '" + generics[binding.formal].name + "' of " + owner);
    }

    void AnalyzerContext::checkPortMap(const ComponentInstantiation& instance, const Symbol& component)
    {
        const auto& ports = component.componentPorts;
        const std::string owner = "instance '" + instance.label + "'";

        AssociationResult result = matchAssociations(ports, associationsOf(instance.portMap), AssociationKind::PortMap, owner, instance);
        if (!result.ok())
            fail(result.problem, *result.at);

        for (const Binding& binding : result.bindings)
            connectPort(binding, ports[binding.formal], instance);
    }

    /// Inputs take any expression of the port type; outputs must be connected to a writable name, which the instance then drives.
    void AnalyzerContext::connectPort(const Binding& binding, const FormalInfo& port, const ComponentInstantiation& instance)
    {
        checkBinding(binding, port, AssociationKind::PortMap, "port '" + port.name + "' of instance '" + instance.label + "'");

        const bool open = dynamic_cast<const OpenExpr*>(binding.actual) != nullptr;
        if (port.mode != PortMode::In && !open)
            recordDriver(*binding.actual, instance);
    }

} // namespace Pulse::Parser
