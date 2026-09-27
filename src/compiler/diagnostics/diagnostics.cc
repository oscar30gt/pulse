#include "diagnostics.h"

namespace Pulse::Parser
{
    namespace
    {
        // ANSI styles of a colored diagnostic (the same choices as GCC and Clang).
        constexpr const char* kBold = "\033[1m";
        constexpr const char* kError = "\033[1;31m";
        constexpr const char* kGutter = "\033[1;34m";
        constexpr const char* kReset = "\033[0m";

        /// `text` in `style`, or plain when there is no color.
        std::string styled(const std::string& text, const char* style, bool color)
        {
            return color ? style + text + kReset : text;
        }

        /// Spaces up to `column`, keeping the tabs of `line` so the caret lands under the same character.
        std::string caretIndent(const std::string& line, size_t column)
        {
            std::string indent;
            for (size_t i = 0; i + 1 < column; ++i)
                indent += i < line.size() && line[i] == '\t' ? '\t' : ' ';
            return indent;
        }
    } // anonymous namespace

    std::string formatDiagnostic(const compiler_error& error, const std::string& path, const std::optional<std::string>& sourceLine,
                                 bool color)
    {
        const SourceLocation& at = error.location();
        const bool located = at.known() && !path.empty();

        std::string text;
        if (located)
            text += styled(path + ":" + std::to_string(at.line) + ":" + std::to_string(at.column) + ":", kBold, color) + " ";
        text += styled(std::string(error.stage()) + " error:", kError, color) + " " + styled(error.what(), kBold, color) + "\n";

        if (!located || !sourceLine)
            return text;

        // `   12 | source` and `      | ^`: the line number right-aligned in a gutter of at least five characters.
        std::string number = std::to_string(at.line);
        if (number.size() < 5)
            number.insert(0, 5 - number.size(), ' ');
        const std::string blank(number.size(), ' ');

        text += styled(number + " |", kGutter, color) + " " + *sourceLine + "\n";
        text += styled(blank + " |", kGutter, color) + " " + caretIndent(*sourceLine, at.column) + styled("^", kGutter, color) + "\n";
        return text;
    }

    std::optional<std::string> sourceLineOf(std::istream& source, size_t line)
    {
        // Lines end at `\n`, `\r\n` or a lone `\r`, as the tokenizer counts them.
        size_t current = 1;
        std::string text;
        char c;
        while (source.get(c))
        {
            if (c != '\n' && c != '\r')
            {
                if (current == line)
                    text += c;
                continue;
            }

            if (c == '\r' && source.peek() == '\n')
                source.get();
            if (current == line)
                return text;
            ++current;
        }

        // The last line has no terminator; an empty file has no line at all.
        if (current == line && (!text.empty() || line > 1))
            return text;
        return std::nullopt;
    }

} // namespace Pulse::Parser
