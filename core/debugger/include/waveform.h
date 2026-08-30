#ifndef PULSE_WAVEFORM_H
#define PULSE_WAVEFORM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "logicVector.h"
#include "subgraph.h"

namespace Pulse::Waveform
{
    // --------------------------------------------------------------------------------------------
    // Waveform Data Model
    // --------------------------------------------------------------------------------------------

    /// Type of signal, used to differentiate between input, output, and internal signals in the waveform.
    enum class SignalType
    {
        Input,      ///< Input port of a circuit or subgraph component.
        Output,     ///< Output port of a circuit or subgraph component.
        Internal    ///< Internal wire/signal within a circuit or subgraph.
    };

    /// A structure representing a single sample of a signal at a specific point in simulation time.
    struct Sample
    {
        LogicVector value;  ///< The logic vector value of the signal at this timestamp.
        uint64_t timestamp; ///< The simulation timestamp of the sample (in femtoseconds).
    };

    /// Represents the full transition trace of a single signal over the course of a simulation.
    struct Wave
    {
        bitWidth_t width : 6;        ///< The bit width of the signal (up to 64 bits).
        SignalType type : 2;         ///< The port/signal type (Input, Output, or Internal).
        std::vector<Sample> samples; ///< Chronological transitions recorded for this signal.

        /// Performs binary search over a chronological sample series to determine the active logic vector
        /// value at a given simulation timestamp.
        /// @param samples Transition list for the signal.
        /// @param time Target timestamp in femtoseconds.
        /// @returns Active logic value, or High-Z if before the first recorded sample.
        LogicVector valueAt(uint64_t time) const;
    };

    /// A hierarchical structure representing the digital circuit waveform, including signals and nested subgraphs.
    struct Waveform
    {
        std::unordered_map<std::string, Wave> signals;       ///< Named signals at the current hierarchy level.
        std::unordered_map<std::string, Waveform> subgraphs; ///< Nested subgraphs representing child components.
    };

    // --------------------------------------------------------------------------------------------
    // Waveform Recorder
    // --------------------------------------------------------------------------------------------

    /// Recorder class that captures snapshots from the simulation engine and incrementally builds a Waveform trace.
    class WaveformRecorder
    {
        Waveform m_waveform; ///< Internal hierarchical waveform representation being populated.

    public:
        /// Constructs a recorder initialized with the signals and hierarchy present in the initial snapshot.
        /// @param initialSnapshot The snapshot of the circuit taken at simulation time 0.
        explicit WaveformRecorder(const Engine::SubgraphSnapshot& initialSnapshot);

        /// Records a state snapshot of signals at a specific simulation timestamp.
        /// Only transitions (value changes) are appended to minimize memory consumption.
        /// @param snapshot The current state of the subgraph's signals.
        /// @param timestamp The simulation timestamp at which the snapshot is captured (in femtoseconds).
        void record(const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp);

        /// Returns a constant reference to the recorded hierarchical waveform trace.
        /// @returns The complete Waveform structure containing all captured signals and transitions.
        [[nodiscard]]
        const Waveform& waveform() const { return m_waveform; }
    };

    // --------------------------------------------------------------------------------------------
    // Formatting and Display Utilities
    // --------------------------------------------------------------------------------------------

    /// Formats the value of a logic vector as a padded hexadecimal string based on its signal bit width.
    /// @param value The LogicVector value to format.
    /// @param width The bit width of the signal, determining hexadecimal zero-padding.
    /// @returns Formatted hexadecimal string (e.g. "0x0A"), or "error" if invalid/high-impedance bits are present.
    [[nodiscard]]
    std::string formatBusValue(const LogicVector& value, bitWidth_t width);
}

#endif // PULSE_WAVEFORM_H