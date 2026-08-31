#include "waveform.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace Pulse::Debugger
{
    // --------------------------------------------------------------------------------------------
    // Sample Lookup Helpers
    // --------------------------------------------------------------------------------------------

    LogicVector Wave::valueAt(simTime_t time) const
    {
        const auto it = std::upper_bound(
            samples.begin(), samples.end(), time,
            [](simTime_t timestamp, const Sample& sample) {
                return timestamp < sample.timestamp;
            }
        );

        if (it == samples.begin())
        {
            return LogicVector::HighZ();
        }

        return std::prev(it)->value;
    }

    // --------------------------------------------------------------------------------------------
    // Waveform Recording Engine
    // --------------------------------------------------------------------------------------------

    /// Class-agnostic recursive helper that recursively records signals 
    /// from a SubgraphSnapshot into a Waveform instance.
    /// Only appends new samples when a signal's value has changed since its last recorded state.
    /// @param waveform The target Waveform structure to update.
    /// @param snapshot The engine snapshot containing current circuit state.
    /// @param timestamp The simulation timestamp of this snapshot in femtoseconds.
    void recordSnapshot(WaveformData& waveform, const Engine::SubgraphSnapshot& snapshot, simTime_t timestamp)
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

    void WaveformRecorder::record(const Engine::SubgraphSnapshot& snapshot, simTime_t timestamp)
    {
        recordSnapshot(m_waveform, snapshot, timestamp);
    }

    const WaveformData& WaveformRecorder::waveform() const
    {
        return m_waveform;
    }
}