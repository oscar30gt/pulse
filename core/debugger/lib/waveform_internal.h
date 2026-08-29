#ifndef PULSE_WAVEFORM_INTERNAL_H
#define PULSE_WAVEFORM_INTERNAL_H

#include "waveform.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace Pulse::Waveform
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
        constexpr const char* focusBg = "\033[48;5;236m"; ///< Subtle dark-gray background for the focused row.
        constexpr const char* busBorder = "\033[1;35m";   ///< Bold magenta for multi-bit bus cell borders.
        constexpr const char* busValue = "\033[1;37m";    ///< Bold white for multi-bit bus hexadecimal labels.
        constexpr const char* hierarchy = "\033[1;34m";   ///< Bold blue for component names and expand/collapse glyphs.
        constexpr const char* treeWhite = "\033[1;37m";   ///< Bright white for tree guides (├─, └─) and signal names.
        constexpr const char* input = "\033[1;36m";       ///< Cyan tag for input signals.
        constexpr const char* output = "\033[1;33m";      ///< Yellow tag for output signals.
        constexpr const char* internal = "\033[2;37m";    ///< Dim gray tag for internal wires.
    }

    // --------------------------------------------------------------------------------------------
    // UTF-8 & String Formatting Helpers
    // --------------------------------------------------------------------------------------------

    /// Computes the visible column width of a UTF-8 encoded string.
    /// @param text The input UTF-8 string.
    /// @returns Visual character width on screen.
    size_t utf8Width(const std::string& text);

    /// Truncates a UTF-8 string so its visible column width does not exceed the given width limit.
    /// @param text The string to truncate.
    /// @param width Maximum visible column width.
    /// @returns Truncated string respecting UTF-8 multi-byte character boundaries.
    std::string truncateUtf8(const std::string& text, size_t width);

    /// Truncates and pads a string with spaces on the right to match the exact target column width.
    /// @param text The string to format.
    /// @param width Target visual width.
    /// @returns Left-aligned, space-padded string.
    std::string fit(std::string text, size_t width);

    /// Truncates and pads a string with spaces on the left to match the exact target column width.
    /// @param text The string to format.
    /// @param width Target visual width.
    /// @returns Right-aligned, space-padded string.
    std::string rightFit(std::string text, size_t width);

    /// Returns the short string tag for a signal type (e.g. "[IN]", "[OUT]", "[INT]").
    /// @param type Signal type enum.
    /// @returns Formatted 6-character type descriptor.
    std::string typeName(SignalType type);

    /// Evaluates the logic value of a signal sample series at a specific timestamp.
    /// @param samples Chronological list of signal transitions.
    /// @param time Target simulation timestamp in femtoseconds.
    /// @returns The active LogicVector at the specified timestamp, or HighZ if before the first transition.
    LogicVector valueAt(const std::vector<Sample>& samples, uint64_t time);

    /// Formats the signal value at the cursor timestamp as a display string.
    /// @param wave The wave trace to sample.
    /// @param time Timestamp in femtoseconds.
    /// @returns '0'/'1' for 1-bit signals, or hexadecimal string for multi-bit buses.
    std::string cursorValue(const Wave& wave, uint64_t time);

    // --------------------------------------------------------------------------------------------
    // Hierarchy Traversal and Row Flattening
    // --------------------------------------------------------------------------------------------

    /// Flattens the hierarchical waveform tree into a linear list of visible rows based on expanded nodes.
    /// Enforces a root component node and connects tree guides for nested elements.
    /// @param waveform The root hierarchical waveform data.
    /// @param expandedPaths Set of component paths that are currently expanded.
    /// @param[out] rows The output list of display rows populated in display order.
    void collectRows(
        const Waveform& waveform,
        const std::unordered_set<std::string>& expandedPaths,
        std::vector<Row>& rows
    );

    // --------------------------------------------------------------------------------------------
    // Waveform Drawing & Glyph Generation
    // --------------------------------------------------------------------------------------------

    /// Renders a single waveform trace row across the given visible time range.
    /// Single-bit signals use level glyphs ('‾', '_', '/', '\\', '.'); buses display hexadecimal runs.
    /// @param wave The signal trace data to render.
    /// @param start Start timestamp of the visible timeline window.
    /// @param end End timestamp of the visible timeline window.
    /// @param cursor Active cursor timestamp.
    /// @param focused True if this row currently has keyboard focus.
    /// @returns ANSI-escaped string for the signal waveform segment.
    std::string renderWave(
        const Wave& wave,
        uint64_t start,
        uint64_t end,
        uint64_t cursor,
        bool focused
    );

    /// Renders the background/cursor space cells for a non-signal row (e.g. component header).
    /// @param start Start timestamp of the visible timeline window.
    /// @param end End timestamp of the visible timeline window.
    /// @param cursor Active cursor timestamp.
    /// @param focused True if this row currently has keyboard focus.
    /// @returns ANSI-escaped string with matching background highlight.
    std::string renderCursor(
        uint64_t start,
        uint64_t end,
        uint64_t cursor,
        bool focused
    );

    /// Assembles a complete frame buffer representing all visible rows, panels, header, and footer.
    /// @param rows List of all active flattened rows.
    /// @param start Visible start timestamp.
    /// @param end Visible end timestamp.
    /// @param cursor Current time cursor position.
    /// @param firstRow First visible row index in the scrollable view.
    /// @param selectedRow Currently focused row index.
    /// @param size Terminal dimensions.
    /// @param controls True to render navigation instructions in the footer.
    /// @returns Vector of formatted strings, one per terminal row.
    std::vector<std::string> buildFrame(
        const std::vector<Row>& rows,
        uint64_t start,
        uint64_t end,
        uint64_t cursor,
        size_t firstRow,
        size_t selectedRow,
        Size size,
        bool controls
    );

    // --------------------------------------------------------------------------------------------
    // Double-Buffered Terminal Output Engine
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
    // Platform-Specific Terminal Lifecycle & Input
    // --------------------------------------------------------------------------------------------

    /// Queries the dimensions of the active console / terminal window.
    /// @returns Size structure with columns and rows, falling back to 80x24 if undetectable.
    Size terminalSize();

    /// Checks if standard input and standard output are attached to an interactive TTY console.
    /// @returns True if running in an interactive terminal, false if piped or redirected.
    bool interactive();

    /// Blocks until a keyboard or resize event is received and maps it to a Key enum.
    /// Handles platform-specific console APIs on Windows and escape sequence parsing on POSIX.
    /// @returns Parsed Key action.
    Key readKey();
}

#endif // PULSE_WAVEFORM_INTERNAL_H
