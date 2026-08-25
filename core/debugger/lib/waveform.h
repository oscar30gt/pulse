#ifndef PULSE_SIGNAL_TRACE_H
#define PULSE_SIGNAL_TRACE_H

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cstddef>

#include "subgraph.h"
#include "logicVector.h"

namespace Pulse::Waveform
{
    enum class SignalType { Input, Output, Internal };

    struct Sample
    {
        LogicVector value;  ///< The value of the signal at this timestamp.
        uint64_t timestamp; ///< The timestamp of the sample.
    };

    /// A structure to hold the state of a signal at a specific point in time.
    struct Wave
    {
        bitWidth_t width : 6; ///< The width of the signal in bits (up to 64 bits).
        SignalType type : 2;
        std::vector<Sample> samples; ///< The samples of the signal at this point in time.
    };

    struct Waveform
    {
        std::unordered_map<std::string, Wave> signals; ///< A map of signal names to their corresponding Wave structures.
        std::unordered_map<std::string, Waveform> subgraphs; ///< A map of subgraph names to their corresponding Waveform structures, allowing for hierarchical representation of signals.
    };

    class WaveformRecorder
    {
        Waveform m_waveform;
    public:
        explicit WaveformRecorder(const Engine::SubgraphSnapshot& initialSnapshot);
        void record(const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp);
        [[nodiscard]] const Waveform& waveform() const { return m_waveform; }
    };

    [[nodiscard]] std::string formatBusValue(const LogicVector& value, bitWidth_t width);
    void showWaveform(const Waveform& waveform, uint64_t startTime, uint64_t endTime);
}

#endif // PULSE_SIGNAL_TRACE_H
