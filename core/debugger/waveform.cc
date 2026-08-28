#include "waveform_internal.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Pulse::Waveform
{
    LogicVector valueAt(const std::vector<Sample>& samples, uint64_t time)
    {
        const auto it = std::upper_bound(
            samples.begin(), samples.end(), time,
            [](uint64_t value, const Sample& sample) {
                return value < sample.timestamp;
            });

        if (it == samples.begin())
        {
            return LogicVector::HighZ();
        }

        return std::prev(it)->value;
    }

    void collectSubRows(
        const Waveform& waveform,
        const std::string& ancestor,
        const std::string& pathPrefix,
        const std::unordered_set<std::string>& expandedPaths,
        std::vector<Row>& rows)
    {
        std::vector<std::pair<std::string, const Wave*>> signals;
        for (const auto& [name, wave] : waveform.signals)
        {
            signals.emplace_back(name, &wave);
        }
        std::sort(signals.begin(), signals.end());

        std::vector<std::pair<std::string, const Waveform*>> graphs;
        for (const auto& [name, graph] : waveform.subgraphs)
        {
            graphs.emplace_back(name, &graph);
        }
        std::sort(graphs.begin(), graphs.end());

        const size_t children = signals.size() + graphs.size();
        size_t index = 0;
        for (const auto& [name, wave] : signals)
        {
            const bool isLast = ++index == children;
            const std::string fullPath = pathPrefix.empty() ? name : pathPrefix + "/" + name;
            rows.push_back({ ancestor + (isLast ? "└─ " : "├─ "), name, wave, fullPath, false });
        }
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

    void collectRows(
        const Waveform& waveform,
        const std::unordered_set<std::string>& expandedPaths,
        std::vector<Row>& rows)
    {
        const bool isExpanded = expandedPaths.find("root") != expandedPaths.end();
        const std::string arrow = isExpanded ? "▼ " : "▶ ";
        rows.push_back({ "", arrow + "root", nullptr, "root", true });
        if (isExpanded)
        {
            collectSubRows(waveform, "", "root", expandedPaths, rows);
        }
    }

    // Append only transitions; repeated snapshots do not grow the trace.
    void recordSnapshot(Waveform& waveform, const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp)
    {
        const auto recordSignals = [&waveform, timestamp](const auto& signals) {
            for (const auto& [name, state] : signals)
            {
                auto it = waveform.signals.find(name);
                if (it != waveform.signals.end() && !it->second.samples.empty() && it->second.samples.back().value != state.second)
                    it->second.samples.push_back({ state.second, timestamp });
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
            m_waveform.subgraphs.emplace(
                name,
                WaveformRecorder(child).waveform());
        }
    }

    void WaveformRecorder::record(const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp)
    {
        recordSnapshot(m_waveform, snapshot, timestamp);
    }

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
