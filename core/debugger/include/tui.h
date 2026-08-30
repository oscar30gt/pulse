#ifndef PULSE_TUI_H
#define PULSE_TUI_H

#include <cstdint>

#include "waveform.h"

namespace Pulse::Debugger
{
    // --------------------------------------------------------------------------------------------
    // Terminal Dimensions and Navigation Input
    // --------------------------------------------------------------------------------------------

    /// Represents the width and height of the active terminal window in characters.
    struct Size
    {
        size_t columns = 80; ///< Number of horizontal columns.
        size_t rows = 24;    ///< Number of vertical rows.
    };

    /// Key action codes parsed from raw terminal keyboard input events.
    enum class Key
    {
        none,   ///< No input received or ignored event.
        resize, ///< Terminal window size changed.
        left,   ///< Left arrow key (time backward).
        right,  ///< Right arrow key (time forward).
        up,     ///< Up arrow key (vertical focus up).
        down,   ///< Down arrow key (vertical focus down).
        toggle, ///< Space or Enter key (expand / collapse component).
        radix,  ///< Tab key (toggle bus radix between hex and binary). Not implemented yet.
        quit    ///< Escape or 'q'/'Q' key (exit waveform viewer).
    };

    // --------------------------------------------------------------------------------------------
    // Row Hierarchy Representation
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

    // --------------------------------------------------------------------------------------------
    // Renderer auxiliary functions
    // --------------------------------------------------------------------------------------------

    /// Differential terminal renderer that only sends changed lines to stdout, preventing visual tearing and flicker.
    class TerminalRenderer
    {
        std::vector<std::string> m_frontBuffer; ///< Previously rendered frame lines.

    public:
        /// Compares the new back buffer against the front buffer, positions the cursor via ANSI escape codes
        /// (`\033[row;colH`), and emits updates for modified lines in a single atomic flush.
        /// @param backBuffer The newly constructed frame lines to draw.
        void render(const std::vector<std::string>& backBuffer);

        /// Invalidates the front buffer, forcing a full repaint on the next render pass (e.g. after resize).
        void clear();
    };

    // --------------------------------------------------------------------------------------------
    // Terminal ANSI Styles
    // --------------------------------------------------------------------------------------------

    /// ANSI color and background escape sequences used for terminal styling.
    namespace Style
    {
        constexpr const char* reset = "\033[0m";         ///< Resets all attributes to terminal defaults.
        constexpr const char* dim = "\033[2;37m";         ///< Dim gray for metadata, timestamps, and row counts.
        constexpr const char* high = "\033[1;32m";        ///< Bold green for high logic levels ('1') and transitions.
        constexpr const char* low = "\033[2;37m";         ///< Dim white for low logic levels ('0').
        constexpr const char* highZ = "\033[1;34m";       ///< Bold blue for high-impedance logic levels ('Z').
        constexpr const char* error = "\033[1;31m";       ///< Bold red for undefined/error logic levels ('X'/'U').
        constexpr const char* cursorBg = "\033[41;1;37m"; ///< Solid red background with bold white text for time cursor.
        constexpr const char* focusBg = "\033[48;5;237m"; ///< Subtle dark-gray background for the focused row.
        constexpr const char* busBorder = "\033[1;35m";   ///< Bold magenta for multi-bit bus cell borders.
        constexpr const char* busValue = "\033[1;37m";    ///< Bold white for multi-bit bus hexadecimal labels.
        constexpr const char* hierarchy = "\033[1;34m";   ///< Bold blue for component names and expand/collapse glyphs.
        constexpr const char* treeWhite = "\033[1;37m";   ///< Bright white for tree guides (├─, └─) and signal names.
        constexpr const char* input = "\033[1;36m";       ///< Cyan tag for input signals.
        constexpr const char* output = "\033[1;33m";      ///< Yellow tag for output signals.
        constexpr const char* internal = "\033[2;37m";    ///< Dim gray tag for internal wires.
    }

    /// Launches the terminal-based interactive waveform viewer (or emits a static frame if non-interactive).
    /// @param waveform The complete hierarchical waveform trace to display.
    /// @param startTime Starting simulation timestamp of the visible range (in femtoseconds).
    /// @param endTime Ending simulation timestamp of the visible range (in femtoseconds).
    /// @param rootName The name of the root component to display. (visual-only, defaults to "root")
    void showWaveform(const WaveformData& waveform, uint64_t startTime, uint64_t endTime, std::string rootName = "root");
}

#endif // PULSE_TUI_H