#include "analyzer_internal.h"

#include <algorithm>

namespace Pulse::Parser
{
    // ---- Recording ------------------------------------------------------------------------------

    /// Names the source that the statements analyzed next drive signals from.
    void AnalyzerContext::beginDriverSource(const Statement& statement)
    {
        m_driverSource = &statement;

        if (auto* process = dynamic_cast<const ProcessStatement*>(&statement))
            m_driverDescription = process->label.empty() ? "an unnamed process" : "process '" + process->label + "'";
        else if (auto* instance = dynamic_cast<const ComponentInstantiation*>(&statement))
            m_driverDescription = "instance '" + instance->label + "'";
        else if (dynamic_cast<const ProcedureCallStatement*>(&statement))
            m_driverDescription = "a concurrent procedure call";
        else
            m_driverDescription = "a concurrent assignment";
    }

    /// Notes that the current source drives the signal `target` names. A process is one driver no
    /// matter how many times it assigns the signal.
    void AnalyzerContext::recordDriver(const Expression& target, const ASTNode& at)
    {
        const RootObject root = rootObject(target);

        // A parameter of a subprogram is only a name for the signal the call passes; the call is what drives it.
        if (!root || root.kind != SymbolKind::Signal || root.isParameter)
            return;

        DriverSet& set = m_drivers[root.objectId];
        if (set.drivers.empty())
        {
            set.name = root.name;
            if (root.symbol)
                set.type = root.symbol->type;
            else if (auto* external = dynamic_cast<const ExternalNameExpr*>(root.node))
                set.type = resolveTypeSpec(*external->subtype);
        }

        auto existing = std::find_if(set.drivers.begin(), set.drivers.end(), [&](const Driver& d) { return d.source == m_driverSource; });
        if (existing != set.drivers.end())
        {
            existing->wholeSignal = existing->wholeSignal || root.whole;
            return;
        }

        set.drivers.push_back({ m_driverSource, m_driverDescription, root.whole, at.source });
    }

    // ---- Checking -------------------------------------------------------------------------------

    /// A signal of an unresolved type (integer, enumerations, records of them, ...) may have only one
    /// source. Element and slice drivers are tracked per source but only conflict with whole-signal
    /// drivers, because two sources may legally drive different elements of an array.
    void AnalyzerContext::checkDrivers()
    {
        std::vector<const DriverSet*> sets;
        for (const auto& entry : m_drivers)
            sets.push_back(&entry.second);

        // Deterministic: the first signal in name order is reported.
        std::sort(sets.begin(), sets.end(), [](const DriverSet* a, const DriverSet* b)
        {
            return a->name != b->name ? a->name < b->name : a->drivers.front().location.line < b->drivers.front().location.line;
        });

        for (const DriverSet* set : sets)
        {
            const auto& drivers = set->drivers;
            const bool resolved = !set->type.info || set->type.info->isResolved || set->type.resolved;
            if (resolved || drivers.size() < 2)
                continue;

            const auto whole = std::count_if(drivers.begin(), drivers.end(), [](const Driver& d) { return d.wholeSignal; });
            if (whole == 0)
                continue;

            std::string sources;
            for (const Driver& driver : drivers)
                sources += (sources.empty() ? "" : ", ") + driver.description;

            fail("Signal '" + set->name + "' has the type '" + describe(set->type) + "', which cannot have several drivers, but it is driven by "
                 + std::to_string(drivers.size()) + " sources (" + sources + "); only std_logic and arrays of it are resolved",
                 drivers[1].location);
        }
    }

} // namespace Pulse::Parser
