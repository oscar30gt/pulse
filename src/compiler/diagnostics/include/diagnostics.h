#ifndef PULSE_VHDL_DIAGNOSTICS_H
#define PULSE_VHDL_DIAGNOSTICS_H

#include <cstddef>
#include <stdexcept>
#include <string>

namespace Pulse::Parser
{
    /// Location of a certain node or token in the source file.
    struct SourceLocation
    {
        size_t line;          /// Line number (1-based)
        size_t column;        /// Column number (1-based)
    };

    // --------------------------------------------------------------------------------------------

    /// Base exception type for every failure raised by the compilation pipeline.
    /// Tokenizer, parser, linker and analyzer errors all derive from it, so callers can
    /// rely on catching this single type for any diagnosable problem in a source file.
    class compiler_error : public std::runtime_error
    {
        SourceLocation m_location;

    public:
        compiler_error(const std::string& message, const SourceLocation& location)
            : std::runtime_error(message), m_location(location) { }

        /// Location of the error inside the source file. Line and column numbers are 1-based.
        const SourceLocation& location() const { return m_location; }
    };

    /// Thrown by the tokenizer for malformed lexical elements (bad numbers, unterminated strings, ...).
    class ast_lexical_error : public compiler_error
    {
    public:
        ast_lexical_error(const std::string& message, const SourceLocation& location)
            : compiler_error(message, location) { }
    };

} // namespace Pulse::Parser

#endif // PULSE_VHDL_DIAGNOSTICS_H
