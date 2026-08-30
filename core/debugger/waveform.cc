#include "waveform.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace Pulse::Waveform
{
    // --------------------------------------------------------------------------------------------
    // Sample Lookup Helpers
    // --------------------------------------------------------------------------------------------

    LogicVector Wave::valueAt(uint64_t time) const
    {
        const auto it = std::upper_bound(
            samples.begin(), samples.end(), time,
            [](uint64_t value, const Sample& sample) {
                return value < sample.timestamp;
            }
        );

        if (it == samples.begin())
        {
            return LogicVector::HighZ();
        }

        return std::prev(it)->value;
    }

    // --------------------------------------------------------------------------------------------
    // Hierarchy Traversal and Row Flattening
    // --------------------------------------------------------------------------------------------

    /// Represents a single display line in the waveform viewer (either a signal or a component node).
    struct Row
    {
        std::string prefix;         ///< Tree connector symbols and indentation (e.g. "│  ├─ ").
        std::string name;           ///< Display name with collapse/expand glyph (e.g. "▶ ALU", "clk").
        const Wave* wave = nullptr; ///< Pointer to the wave trace data, or nullptr if this row is a component.
        std::string path;           ///< Unique hierarchical path identifier (e.g. "root/CPU/ALU").
        bool isGraph = false;       ///< True if this row represents a collapsible component/subgraph node.
    };

    /// Internal recursive helper that flattens signals and subgraphs belonging to a specific component.
    /// @param waveform The waveform data of the component being traversed.
    /// @param ancestor Tree guide string for parent levels (e.g. "│  ").
    /// @param pathPrefix Unique hierarchical prefix identifying this component (e.g. "root/CPU").
    /// @param expandedPaths Set of paths representing currently expanded component nodes.
    /// @param[out] rows Destination vector populated with display rows.
    void collectSubRows(
        const Waveform& waveform,
        const std::string& ancestor,
        const std::string& pathPrefix,
        const std::unordered_set<std::string>& expandedPaths,
        std::vector<Row>& rows
    )
    {
        // 1. Collect and sort all signal names alphabetically at this hierarchy level
        std::vector<std::pair<std::string, const Wave*>> signals;
        for (const auto& [name, wave] : waveform.signals)
        {
            signals.emplace_back(name, &wave);
        }
        std::sort(signals.begin(), signals.end());

        // 2. Collect and sort all child subgraphs alphabetically
        std::vector<std::pair<std::string, const Waveform*>> graphs;
        for (const auto& [name, graph] : waveform.subgraphs)
        {
            graphs.emplace_back(name, &graph);
        }
        std::sort(graphs.begin(), graphs.end());

        const size_t children = signals.size() + graphs.size();
        size_t index = 0;

        // 3. Emit rows for signals
        for (const auto& [name, wave] : signals)
        {
            const bool isLast = ++index == children;
            const std::string fullPath = pathPrefix.empty() ? name : pathPrefix + "/" + name;
            rows.push_back({ ancestor + (isLast ? "└─ " : "├─ "), name, wave, fullPath, false });
        }

        // 4. Emit rows for subgraphs and recursively expand if node is in expandedPaths
        for (const auto& [name, graph] : graphs)
        {
            const bool isLast = ++index == children;
            const std::string fullPath = pathPrefix.empty() ? name : pathPrefix + "/" + name;
            const bool isExpanded = expandedPaths.find(fullPath) != expandedPaths.end();
            const std::string prefix = ancestor + (isLast ? "└─ " : "├─ ");
            const std::string arrow = isExpanded ? "▼ " : "▶ ";
            rows.push_back({ prefix, arrow + name, nullptr, fullPath, true });

            if (isExpanded)
            {
                collectSubRows(*graph, ancestor + (isLast ? "   " : "│  "), fullPath, expandedPaths, rows);
            }
        }
    }

    /// Flattens the entire circuit hierarchy under a top-level fake `root` component.
    /// If `root` is expanded, all top-level signals and components are emitted.
    /// @param waveform The top-level circuit waveform.
    /// @param expandedPaths Set of component paths currently open in the viewer.
    /// @param[out] rows Output vector populated with flattened display rows.
    void collectRows(
        const Waveform& waveform,
        const std::unordered_set<std::string>& expandedPaths,
        std::vector<Row>& rows
    )
    {
        const bool isExpanded = expandedPaths.find("root") != expandedPaths.end();
        const std::string arrow = isExpanded ? "▼ " : "▶ ";
        rows.push_back({ "", arrow + "root", nullptr, "root", true });

        if (isExpanded)
        {
            collectSubRows(waveform, "", "root", expandedPaths, rows);
        }
    }

    // --------------------------------------------------------------------------------------------
    // Waveform Recording Engine
    // --------------------------------------------------------------------------------------------

    /// Recursively records signals from a SubgraphSnapshot into a Waveform instance.
    /// Only appends new samples when a signal's value has changed since its last recorded state.
    /// @param waveform The target Waveform structure to update.
    /// @param snapshot The engine snapshot containing current circuit state.
    /// @param timestamp The simulation timestamp of this snapshot in femtoseconds.
    void recordSnapshot(Waveform& waveform, const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp)
    {
        const auto recordSignals = [&waveform, timestamp](const auto& signals) {
            for (const auto& [name, state] : signals)
            {
                auto it = waveform.signals.find(name);
                if (it != waveform.signals.end() && !it->second.samples.empty() && it->second.samples.back().value != state.second)
                {
                    it->second.samples.push_back({ state.second, timestamp });
                }
            }
        };

        recordSignals(snapshot.inputs);
        recordSignals(snapshot.outputs);
        recordSignals(snapshot.wires);

        for (const auto& [name, child] : snapshot.subgraphs)
        {
            if (auto it = waveform.subgraphs.find(name); it != waveform.subgraphs.end())
            {
                recordSnapshot(it->second, child, timestamp);
            }
        }
    }

    WaveformRecorder::WaveformRecorder(const Engine::SubgraphSnapshot& snapshot)
    {
        const auto add = [this](const auto& signals, SignalType type) {
            for (const auto& [name, state] : signals)
            {
                m_waveform.signals.emplace(
                    name,
                    Wave{ state.first, type, {{ state.second, 0 }} });
            }
        };

        add(snapshot.inputs, SignalType::Input);
        add(snapshot.outputs, SignalType::Output);
        add(snapshot.wires, SignalType::Internal);

        for (const auto& [name, child] : snapshot.subgraphs)
        {
            m_waveform.subgraphs.emplace(name, WaveformRecorder(child).waveform());
        }
    }

    void WaveformRecorder::record(const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp)
    {
        recordSnapshot(m_waveform, snapshot, timestamp);
    }

    // --------------------------------------------------------------------------------------------
    // Bus Value Formatting
    // --------------------------------------------------------------------------------------------

    std::string formatBusValue(const LogicVector& value, bitWidth_t width)
    {
        const uint8_t bits = std::min<uint8_t>(width, 64);
        for (uint8_t bit = 0; bit < bits; ++bit)
        {
            if (value.bit(bit) != '0' && value.bit(bit) != '1')
            {
                return "error";
            }
        }

        std::ostringstream out;
        const uint64_t widthMask = bits == 64 ? ~uint64_t{ 0 } : ((uint64_t{ 1 } << bits) - 1);
        out << "0x"
            << std::uppercase << std::hex
            << std::setw(static_cast<int>(std::max<size_t>(1, (bits + 3) / 4)))
            << std::setfill('0') << (value.value & widthMask);
        return out.str();
    }
}