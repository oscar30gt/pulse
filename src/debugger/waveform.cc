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

    Engine::LogicVector Wave::valueAt(simTime_t time) const
    {
        const auto it = std::upper_bound(
            samples.begin(), samples.end(), time,
            [](simTime_t timestamp, const Sample& sample) {
                return timestamp < sample.timestamp;
            }
        );

        if (it == samples.begin())
        {
            return Engine::LogicVector::HighZ();
        }

        return std::prev(it)->value;
    }

    bool Wave::isLevel() const
    {
        return width == 1 && (display.format == Engine::DisplayFormat::Bit || display.format == Engine::DisplayFormat::Boolean);
    }

    bool Wave::isExpandable() const
    {
        return width > 1 && display.format == Engine::DisplayFormat::Hex;
    }

    // --------------------------------------------------------------------------------------------
    // Value formatting
    // --------------------------------------------------------------------------------------------

    namespace
    {
        uint64_t maskOf(bitWidth_t width)
        {
            return width >= 64 ? ~uint64_t{ 0 } : ((uint64_t{ 1 } << width) - 1);
        }

        /// Decimal digits of the largest magnitude a number of `width` bits can have.
        size_t decimalDigits(bitWidth_t width, bool isSigned)
        {
            const uint64_t largest = isSigned ? (uint64_t{ 1 } << (width - 1)) : maskOf(width);
            return std::to_string(largest).size() + (isSigned ? 1 : 0);
        }
    } // anonymous namespace

    std::string formatValue(const Wave& wave, const Engine::LogicVector& value)
    {
        const bitWidth_t width = std::min<bitWidth_t>(wave.width, 64);
        const Engine::LogicVector bits = value.range(width);

        switch (wave.display.format)
        {
            case Engine::DisplayFormat::Bit:
                return std::string(1, bits.bit(0));

            case Engine::DisplayFormat::Boolean:
                if (!bits.isDefinite()) return "X";
                return bits.value & 1 ? "true" : "false";

            case Engine::DisplayFormat::SignedInteger:
            case Engine::DisplayFormat::UnsignedInteger:
            {
                if (!bits.isDefinite()) return "X";
                if (wave.display.format == Engine::DisplayFormat::UnsignedInteger)
                    return std::to_string(bits.value);
                const uint64_t sign = uint64_t{ 1 } << (width - 1);
                const int64_t number = width >= 64 ? static_cast<int64_t>(bits.value)
                                                   : static_cast<int64_t>((bits.value ^ sign) - sign);
                return std::to_string(number);
            }

            case Engine::DisplayFormat::Enumeration:
                if (!bits.isDefinite() || bits.value >= wave.display.literals.size()) return "X";
                return wave.display.literals[static_cast<size_t>(bits.value)];

            case Engine::DisplayFormat::Hex:
                break;
        }

        if (!bits.isDefinite())
            return "error";

        std::ostringstream out;
        out << "0x" << std::uppercase << std::hex
            << std::setw(static_cast<int>(std::max<size_t>(1, (width + 3) / 4)))
            << std::setfill('0') << bits.value;
        return out.str();
    }

    size_t formatWidth(const Wave& wave)
    {
        switch (wave.display.format)
        {
            case Engine::DisplayFormat::Bit:
                return 1;
            case Engine::DisplayFormat::Boolean:
                return 5;
            case Engine::DisplayFormat::SignedInteger:
                return decimalDigits(wave.width, true);
            case Engine::DisplayFormat::UnsignedInteger:
                return decimalDigits(wave.width, false);
            case Engine::DisplayFormat::Enumeration:
            {
                size_t longest = 1;
                for (const std::string& literal : wave.display.literals)
                    longest = std::max(longest, literal.size());
                return longest;
            }
            case Engine::DisplayFormat::Hex:
                break;
        }
        return std::max<size_t>(5, (wave.width + 3) / 4 + 2);
    }

    Wave bitWave(const Wave& bus, uint8_t bit)
    {
        Wave wave;
        wave.width = 1;
        wave.type = bus.type;
        wave.display.format = Engine::DisplayFormat::Bit;

        for (const Sample& sample : bus.samples)
        {
            const Engine::LogicVector value = sample.value.range(bit, bit);
            if (wave.samples.empty() || wave.samples.back().value != value)
                wave.samples.push_back({ value, sample.timestamp });
        }
        return wave;
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
        const auto add = [this, &snapshot](const auto& signals, SignalType type) {
            for (const auto& [name, state] : signals)
            {
                Wave wave;
                wave.width = state.first;
                wave.type = type;
                wave.samples = { { state.second, 0 } };

                const Engine::SignalSymbol* symbol = snapshot.symbols ? snapshot.symbols->find(name) : nullptr;
                if (symbol)
                    wave.display = *symbol;
                else
                    wave.display.format = state.first == 1 ? Engine::DisplayFormat::Bit : Engine::DisplayFormat::Hex;

                m_waveform.signals.emplace(name, std::move(wave));
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
