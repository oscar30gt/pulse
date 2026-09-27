#ifndef PULSE_VHDL_DIAGNOSTICS_H
#define PULSE_VHDL_DIAGNOSTICS_H

#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>

namespace Pulse::Parser
{
    /// File index of a location that belongs to no source file (the prelude, or a source tokenized without one).
    constexpr size_t noFile = SIZE_MAX;

    /// Location of a certain node or token in the source files of a design.
    struct SourceLocation
    {
        size_t line = 0;        ///< Line number (1-based); 0 when the location is unknown
        size_t column = 0;      ///< Column number (1-based, counted in bytes)
        size_t file = noFile;   ///< Index of the source file, as given to its Tokenizer; noFile when there is none

        /// Whether the location points somewhere. Errors about the design as a whole (no top entity ...) have none.
        bool known() const { return line != 0; }
    };

    // --------------------------------------------------------------------------------------------

    /// Base exception type for every failure raised by the compilation pipeline.
    /// Tokenizer, parser, analyzer, linker and elaborator errors all derive from it, so callers can
    /// rely on catching this single type for any diagnosable problem in a design.
    class compiler_error : public std::runtime_error
    {
        SourceLocation m_location;

    public:
        compiler_error(const std::string& message, const SourceLocation& location)
            : std::runtime_error(message), m_location(location) { }

        /// Location of the error inside the design. Line and column numbers are 1-based.
        const SourceLocation& location() const { return m_location; }

        /// The stage of the pipeline that raised the error ("lexical", "syntax", "semantic", "link", "elaboration").
        virtual const char* stage() const { return "compiler"; }
    };

    /// Thrown by the tokenizer for malformed lexical elements (bad numbers, unterminated strings, ...).
    class ast_lexical_error : public compiler_error
    {
    public:
        ast_lexical_error(const std::string& message, const SourceLocation& location)
            : compiler_error(message, location) { }

        const char* stage() const override { return "lexical"; }
    };

    // --------------------------------------------------------------------------------------------

    /// Formats a diagnostic the way GCC and Clang do:
    ///
    ///     path:line:column: <stage> error: message
    ///        12 |     source line
    ///           |     ^
    ///
    /// Without a known location only the `<stage> error: message` line is written, and without a source line only the first
    /// line. The caret keeps the tabs of the source line so it stays under the column.
    /// @param error The diagnostic.
    /// @param path The file the location refers to, or "" when there is none.
    /// @param sourceLine The text of the line the location refers to, if it could be read.
    /// @param color Whether to color the text with ANSI escape sequences (for a terminal).
    /// @returns The formatted diagnostic, ending with a newline.
    std::string formatDiagnostic(const compiler_error& error, const std::string& path, const std::optional<std::string>& sourceLine,
                                 bool color);

    /// Reads line `line` (1-based) of `source`, without its line terminator (`\n`, `\r\n` or `\r`).
    /// @returns The text of the line, or nothing when the source has fewer lines.
    std::optional<std::string> sourceLineOf(std::istream& source, size_t line);

} // namespace Pulse::Parser

#endif // PULSE_VHDL_DIAGNOSTICS_H
