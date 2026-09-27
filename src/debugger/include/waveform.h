#ifndef PULSE_WAVEFORM_H
#define PULSE_WAVEFORM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "logicVector.h"
#include "subgraph.h"
#include "symbolTable.h"

namespace Pulse::Debugger
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
        Engine::LogicVector value;  ///< The logic vector value of the signal at this timestamp.
        simTime_t timestamp; ///< The simulation timestamp of the sample (in femtoseconds).
    };

    /// Represents the full transition trace of a single signal over the course of a simulation.
    struct Wave
    {
        bitWidth_t width = 1;           ///< The bit width of the signal (up to 64 bits).
        SignalType type = SignalType::Internal;     ///< The port/signal type (Input, Output, or Internal).
        Engine::SignalSymbol display;   ///< How its values are shown (from the symbol table of the simulated design).
        std::vector<Sample> samples;    ///< Chronological transitions recorded for this signal.

        /// Performs binary search over a chronological sample series to determine the active logic vector
        /// value at a given simulation timestamp.
        /// @param time Target timestamp in femtoseconds.
        /// @returns Active logic value, or High-Z if before the first recorded sample.
        Engine::LogicVector valueAt(simTime_t time) const;

        /// Whether the wave is drawn as a single line of levels (a bit or a boolean) rather than as a bus of labeled values.
        bool isLevel() const;

        /// Whether the wave is a logic vector that can be expanded into one wave per bit.
        bool isExpandable() const;
    };

    /// A hierarchical structure representing the digital circuit waveform, including signals and nested subgraphs.
    struct WaveformData
    {
        std::unordered_map<std::string, Wave> signals;       ///< Named signals at the current hierarchy level.
        std::unordered_map<std::string, WaveformData> subgraphs; ///< Nested subgraphs representing child components.
    };

    // --------------------------------------------------------------------------------------------
    // Value formatting
    // --------------------------------------------------------------------------------------------

    /// The text of a value of a wave, as its display format asks: a bit ('0', '1', 'X', 'Z'), "true"/"false", a
    /// hexadecimal bus ("0x3F", or "error" when a bit is not '0' or '1'), a decimal integer, or an enumeration literal.
    /// A number or a literal that holds an unknown bit reads "X".
    std::string formatValue(const Wave& wave, const Engine::LogicVector& value);

    /// The widest text formatValue() can give for the wave, in characters.
    size_t formatWidth(const Wave& wave);

    /// The wave of one bit of a logic vector wave: its transitions only, shown as a bit.
    /// @param bus The vector wave.
    /// @param bit Position of the bit, 0 being the least significant.
    Wave bitWave(const Wave& bus, uint8_t bit);

    // --------------------------------------------------------------------------------------------
    // Waveform Recorder
    // --------------------------------------------------------------------------------------------

    /// Recorder class that captures snapshots from the simulation engine and incrementally builds a Waveform trace.
    class WaveformRecorder
    {
        WaveformData m_waveform; ///< Internal hierarchical waveform representation being populated.

    public:
        /// Constructs a recorder initialized with the signals and hierarchy present in the initial snapshot.
        /// Every signal takes its display format from the snapshot's symbol table; a signal it does not describe is shown
        /// as a bit (one bit wide) or as a hexadecimal bus.
        /// @param initialSnapshot The snapshot of the circuit taken at simulation time 0.
        explicit WaveformRecorder(const Engine::SubgraphSnapshot& initialSnapshot);

        /// Records a state snapshot of signals at a specific simulation timestamp.
        /// Only transitions (value changes) are appended to minimize memory consumption.
        /// @param snapshot The current state of the subgraph's signals.
        /// @param timestamp The simulation timestamp at which the snapshot is captured (in femtoseconds).
        void record(const Engine::SubgraphSnapshot& snapshot, simTime_t timestamp);

        /// Returns a constant reference to the recorded hierarchical waveform trace.
        /// @returns The complete Waveform structure containing all captured signals and transitions.
        [[nodiscard]]
        const WaveformData& waveform() const;
    };
}

#endif // PULSE_WAVEFORM_H
