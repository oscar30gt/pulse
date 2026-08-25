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
    /// Type of signal, used to differentiate between input, output, and internal signals in the waveform.
    enum class SignalType { Input, Output, Internal };

    /// A structure to represent a single sample of a signal at a specific timestamp.
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

    /// A structure to represent the waveform of a digital circuit, including its signals and subgraphs.
    struct Waveform
    {
        std::unordered_map<std::string, Wave> signals; ///< A map of signal names to their corresponding Wave structures.
        std::unordered_map<std::string, Waveform> subgraphs; ///< A map of subgraph names to their corresponding Waveform structures, allowing for hierarchical representation of signals.
    };

    /// Helper class to record the state of signals over time, allowing for the creation of a waveform representation of a digital circuit's behavior.
    class WaveformRecorder
    {
        Waveform m_waveform;

    public:
        explicit WaveformRecorder(const Engine::SubgraphSnapshot& initialSnapshot);
        
        /// Records the state of signals at a specific timestamp, updating only those signals that have changed since the last recorded state.
        /// @param snapshot The current state of the subgraph's signals to be recorded.
        /// @param timestamp The timestamp at which the snapshot is taken, used to track when changes occur in the waveform.
        void record(const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp);
        
        /// Returns a constant reference to the recorded waveform.
        /// @returns Waveform generated during the recording process.
        [[nodiscard]] 
        const Waveform& waveform() const { return m_waveform; }
    };

    /// Formats the value of a logic vector as a hexadecimal string, taking into account the specified bit width.
    /// @param value The LogicVector value to be formatted.
    /// @param width The bit width of the signal, which determines how many bits of the LogicVector are considered for formatting.
    /// @returns A string representing the formatted value of the LogicVector in hexadecimal notation, or "error" if the value contains invalid bits.
    [[nodiscard]] 
    std::string formatBusValue(const LogicVector& value, bitWidth_t width);
    
    /// Displays a terminal-based interactive waveform visualization, similar to gtkwave, but cli-based.
    void showWaveform(const Waveform& waveform, uint64_t startTime, uint64_t endTime);
}

#endif // PULSE_SIGNAL_TRACE_H
