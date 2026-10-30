#ifndef PULSE_VCD_H
#define PULSE_VCD_H

#include <cstddef>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "types.h"
#include "waveform.h"

namespace Pulse::Debugger
{
    // --------------------------------------------------------------------------------------------
    // Value Change Dump (IEEE 1364, section 18)
    // --------------------------------------------------------------------------------------------
    //
    // A VCD file is how Pulse exchanges waveforms with other simulators and viewers (GHDL, Icarus Verilog,
    // ModelSim, GTKWave, browser viewers): `pulse --out` writes the simulated waveform as one, and `pulse --in`
    // shows one in the waveform viewer.
    //
    // The hierarchy maps to scopes: the root component is the top `$scope module`, and every subgraph is a scope
    // inside its parent. Time is written in femtoseconds (`$timescale 1 fs`), the tick of the engine.
    //
    // What VCD cannot say (the port direction of a signal and how its values are displayed: booleans, integers,
    // enumeration literals) is kept in one `$comment pulse-signal <id> <direction> <format> [literals] $end` after
    // each `$var`. Other tools ignore comments, and Pulse reads the file back exactly as it was simulated. A file
    // without these comments is shown the plain VCD way: one bit, or a vector in hexadecimal (an `integer` in decimal).

    /// A waveform read from a VCD file.
    struct VcdTrace
    {
        WaveformData waveform;              ///< The signals and scopes of the file, below the root.
        std::string rootName = "root";      ///< The name of the root: the only top-level scope, or "root" when there are several.
        simTime_t endTime = 0;              ///< The last timestamp of the file, in femtoseconds.
        std::vector<std::string> skipped;   ///< Why each variable Pulse cannot show (real, string, wider than 64 bits) was left out.
    };

    /// A malformed VCD file, at a line of it.
    class vcd_error : public std::runtime_error
    {
        size_t m_line;

    public:
        vcd_error(size_t line, const std::string& message) : std::runtime_error(message), m_line(line) { }

        /// The line of the file (1 is the first one) where the error is.
        size_t line() const { return m_line; }
    };

    /// Writes a waveform as a VCD file: the hierarchy under the root as scopes (signals and scopes by name), the
    /// initial values in `$dumpvars`, then every transition, and a last timestamp at the end time.
    /// @param out The stream to write to.
    /// @param waveform The waveform to write.
    /// @param rootName The name of the top scope (the top entity).
    /// @param endTime The end of the simulation, in femtoseconds; a timestamp later than every transition marks it.
    void writeVcd(std::ostream& out, const WaveformData& waveform, const std::string& rootName, simTime_t endTime);

    /// Reads a VCD file. Scalar and vector values take the four logic values of the engine: `0`/`l` are 0, `1`/`h` are
    /// 1, `z` is Z, and `x`/`u`/`w`/`-` are X (any case). A vector shorter than its variable is extended on the left
    /// as IEEE 1364 says. Variables Pulse cannot show (real and string values, more than 64 bits) are skipped, and
    /// listed in VcdTrace::skipped.
    /// @param in The stream to read the file from.
    /// @returns The waveform of the file, its root name and its end time.
    /// @throws vcd_error for a malformed file.
    VcdTrace readVcd(std::istream& in);

    /// Whether a path names a VCD file: its extension is `.vcd`, in any case.
    bool isVcdPath(const std::string& path);
}

#endif // PULSE_VCD_H
