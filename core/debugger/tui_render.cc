#include "tui.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace Pulse::Debugger
{

    // --------------------------------------------------------------------------------------------
    // UTF-8 & String Formatting Helpers
    // --------------------------------------------------------------------------------------------

    std::string formatBusValue(const LogicVector& value, bitWidth_t width)
    {
        const uint8_t bits = std::min<uint8_t>(width, 64);
        for (uint8_t bit = 0; bit < bits; ++bit)
        {
            if (value.bit(bit) != '0' && value.bit(bit) != '1')
            {
                return "error";
            }
        }

        std::ostringstream out;
        const uint64_t widthMask = bits == 64 ? ~uint64_t{ 0 } : ((uint64_t{ 1 } << bits) - 1);
        out << "0x"
            << std::uppercase << std::hex
            << std::setw(static_cast<int>(std::max<size_t>(1, (bits + 3) / 4)))
            << std::setfill('0') << (value.value & widthMask);
        return out.str();
    }

    size_t utf8Width(const std::string& text)
    {
        size_t width = 0;
        for (size_t i = 0; i < text.size(); ++i)
        {
            // Skip UTF-8 continuation bytes (10xxxxxx)
            if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80)
            {
                ++width;
            }
        }
        return width;
    }

    std::string truncateUtf8(const std::string& text, size_t width)
    {
        if (utf8Width(text) <= width)
        {
            return text;
        }

        std::string result;
        size_t columns = 0;
        for (size_t i = 0; i < text.size() && columns < width;)
        {
            const unsigned char byte = static_cast<unsigned char>(text[i]);
            const size_t length = byte < 0x80 ? 1 : (byte < 0xE0 ? 2 : (byte < 0xF0 ? 3 : 4));
            if (i + length > text.size()) break;
            result.append(text, i, length);
            i += length;
            ++columns;
        }
        return result;
    }

    std::string fit(std::string text, size_t width)
    {
        text = truncateUtf8(text, width);
        return text.append(width - utf8Width(text), ' ');
    }

    std::string rightFit(std::string text, size_t width)
    {
        text = truncateUtf8(text, width);
        return std::string(width - utf8Width(text), ' ') + text;
    }

    std::string typeName(SignalType type)
    {
        switch (type)
        {
            case SignalType::Input:
                return "[IN]";
            case SignalType::Output:
                return "[OUT]";
            case SignalType::Internal:
                return "[INT]";
        }
        return {};
    }

    std::string cursorValue(const Wave& wave, simTime_t time)
    {
        const LogicVector value = wave.valueAt(time);
        if (wave.width == 1)
        {
            return std::string(1, value.bit(0));
        }

        return formatBusValue(value, wave.width);
    }

    // --------------------------------------------------------------------------------------------
    // Waveform Drawing & Glyph Generation
    // --------------------------------------------------------------------------------------------

    std::string renderWave(const Wave& wave, simTime_t start, simTime_t end, simTime_t cursor, bool focused)
    {
        std::ostringstream out;
        const char* defaultBg = focused ? Style::focusBg : "";

        // Single-bit signals: use level glyphs and transition slopes
        if (wave.width == 1)
        {
            char previous = wave.valueAt(start).bit(0);
            for (simTime_t time = start; time < end; ++time)
            {
                const char current = wave.valueAt(time).bit(0);
                const char next = wave.valueAt(time + 1).bit(0);
                const char* color = Style::low;
                std::string glyph = "_";

                if (current == '1')
                {
                    color = Style::high;
                    // Falling edge slope on the last '1' before a '0' transition
                    if (next != '1' && previous == '1')
                    {
                        glyph = "\\";
                    }
                    // Rising edge slope on the first '1' following a '0'
                    else if (previous != '1')
                    {
                        glyph = "/";
                    }
                    // Sustained high level
                    else
                    {
                        glyph = "‾";
                    }
                }
                else if (current == '0')
                {
                    color = Style::low;
                    glyph = "_";
                }
                else if (current == 'Z')
                {
                    color = Style::highZ;
                    glyph = ".";
                }
                else
                {
                    color = Style::error;
                    glyph = "?";
                }

                // Cursor highlight takes precedence over normal background styling
                if (time == cursor)
                {
                    out << Style::cursorBg << glyph << Style::reset;
                }
                else
                {
                    out << defaultBg << color << glyph << Style::reset;
                }
                previous = current;
            }
            return out.str();
        }

        // Multi-bit buses: divide timestamps into runs of identical values and render hex labels
        for (simTime_t time = start; time < end;)
        {
            const LogicVector value = wave.valueAt(time);
            simTime_t run = 1;
            while (time + run < end && wave.valueAt(time + run) == value)
            {
                ++run;
            }

            const size_t room = static_cast<size_t>(run - 1);
            std::string cells(static_cast<size_t>(run), ' ');
            cells[0] = '|';

            if (room > 0)
            {
                std::string label = formatBusValue(value, wave.width);
                if (label.size() > room)
                {
                    label.resize(room);
                }

                std::copy(label.begin(), label.end(), cells.begin() + 1);
            }

            const size_t cursorOffset = cursor >= time && cursor < time + run ? static_cast<size_t>(cursor - time) : run;

            for (size_t i = 0; i < cells.size(); ++i)
            {
                const bool cursorCell = i == cursorOffset;
                const bool borderCell = i == 0;
                const bool errorCell = !cursorCell && formatBusValue(value, wave.width) == "error";
                const char* color = cursorCell
                    ? Style::cursorBg
                    : (errorCell ? Style::error : (borderCell ? Style::busBorder : Style::busValue));

                if (cursorCell)
                {
                    out << color << cells[i] << Style::reset;
                }
                else
                {
                    out << defaultBg << color << cells[i] << Style::reset;
                }
            }
            time += run;
        }
        return out.str();
    }

    std::string renderCursor(simTime_t start, simTime_t end, simTime_t cursor, bool focused)
    {
        std::string result;
        const char* defaultBg = focused ? Style::focusBg : "";

        for (simTime_t time = start; time < end; ++time)
        {
            if (time == cursor)
            {
                result += std::string(Style::cursorBg) + " " + Style::reset;
            }
            else if (focused)
            {
                result += std::string(defaultBg) + " " + Style::reset;
            }
            else
            {
                result += " ";
            }
        }
        return result;
    }

    std::vector<std::string> buildFrame(
        const std::vector<Row>& rows,
        simTime_t start,
        simTime_t end,
        simTime_t cursor,
        size_t firstRow,
        size_t selectedRow,
        Size size,
        bool controls
    )
    {
        std::vector<std::string> frame;

        // Dynamic column width calculation
        const size_t panelWidth = std::clamp(size.columns / 3, size_t{ 46 }, size_t{ 60 });
        size_t valueWidth = 3; // Default width.

        // Dynamic value width calculation based on the widest bus value in the visible rows (up to 18 characters).
        for (const Row& row : rows) if (row.wave)
        {
            valueWidth = std::max(
                valueWidth,
                row.wave->width == 1
                    ? size_t{ 1 }
                    : std::min<size_t>(18, (row.wave->width + 3) / 4 + 2)
            );
        }

        const size_t typeWidth = 6;
        const size_t nameWidth = panelWidth > valueWidth + typeWidth + 2
            ? panelWidth - valueWidth - typeWidth - 2
            : 8;
        const size_t timeWidth = std::max<size_t>(
            1,
            size.columns > panelWidth + 3 ? size.columns - panelWidth - 3 : 1);

        const simTime_t visibleEnd = std::min(end, start + timeWidth);
        const size_t visibleRows = std::max<size_t>(1, size.rows > 3 ? size.rows - 3 : 1);

        // Header line
        std::ostringstream line;
        line << Style::dim << rightFit("Cursor: " + std::to_string(cursor) + "fs", panelWidth)
            << " │ Time " << start << ".." << (visibleEnd - 1) << Style::reset;
        frame.push_back(line.str());

        // Visible signal and component rows
        for (size_t i = firstRow; i < rows.size() && i < firstRow + visibleRows; ++i)
        {
            line.str("");
            line.clear();
            const Row& row = rows[i];
            const bool focused = (i == selectedRow);
            const char* bg = focused ? Style::focusBg : "";

            if (!row.wave)
            {
                // For collapsible components: tree lines are drawn in white,
                // while only the arrow and component name are highlighted in hierarchy blue.
                const size_t prefixLen = utf8Width(row.prefix);
                const std::string truncatedName = truncateUtf8(
                    row.name,
                    nameWidth > prefixLen ? nameWidth - prefixLen : 0);
                const size_t totalLen = prefixLen + utf8Width(truncatedName);
                const std::string padding = totalLen < nameWidth
                    ? std::string(nameWidth - totalLen, ' ')
                    : "";

                line << bg
                    << Style::treeWhite << row.prefix
                    << Style::hierarchy << truncatedName
                    << padding << Style::reset
                    << bg << ' ' << fit("", typeWidth)
                    << ' ' << fit("", valueWidth) << Style::reset
                    << " │" << renderCursor(start, visibleEnd, cursor, focused);
            }
            else
            {
                const std::string value = cursor < end ? cursorValue(*row.wave, cursor) : "";
                line << bg << Style::treeWhite << fit(row.prefix + row.name, nameWidth)
                    << ' ' << Style::dim << fit(typeName(row.wave->type), typeWidth)
                    << ' ' << rightFit(value, valueWidth) << Style::reset
                    << " │" << renderWave(*row.wave, start, visibleEnd, cursor, focused);
            }
            frame.push_back(line.str());
        }

        // 3. Footer line with row counts and keyboard navigation shortcuts
        line.str("");
        line.clear();
        line << Style::dim
            << "Rows " << (rows.empty() ? 0 : firstRow + 1)
            << '-' << std::min(rows.size(), firstRow + visibleRows)
            << '/' << rows.size();
        if (controls)
        {
            line << "  fs  Arrows: move cursor/focus  Space/Enter: collapse/expand  Q/Esc: quit";
        }
        line << Style::reset;
        frame.push_back(line.str());

        return frame;
    }

    // --------------------------------------------------------------------------------------------
    // Double-Buffered Terminal Output Engine
    // --------------------------------------------------------------------------------------------

    void TerminalRenderer::render(const std::vector<std::string>& backBuffer)
    {
        std::ostringstream batch;

        // Differential update: reposition cursor and write only lines that differ from frontBuffer
        for (size_t i = 0; i < backBuffer.size(); ++i)
        {
            if (i >= m_frontBuffer.size() || m_frontBuffer[i] != backBuffer[i])
            {
                batch << "\033[" << (i + 1) << ";1H"
                    << backBuffer[i]
                    << "\033[K";
            }
        }

        // Clear trailing lines if new frame has fewer rows than previous frame
        if (m_frontBuffer.size() > backBuffer.size())
        {
            batch << "\033[" << (backBuffer.size() + 1) << ";1H\033[J";
        }

        // Atomic flush of entire batch to stdout
        std::cout << batch.str() << std::flush;
        m_frontBuffer = backBuffer;
    }

    /// Invalidates the front buffer, forcing a full repaint on the next render pass (e.g. after resize).
    void TerminalRenderer::clear()
    {
        m_frontBuffer.clear();
    }
}