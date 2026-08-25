#include "waveform.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace Pulse::Waveform
{
    namespace
    {
        // ANSI styles used by the terminal renderer. Keep these in one place so
        // the drawing code below reads in terms of signal meaning, not escapes.
        constexpr const char* reset = "\033[0m";
        constexpr const char* dim = "\033[2;37m";
        constexpr const char* high = "\033[1;32m";
        constexpr const char* low = "\033[2;37m";
        constexpr const char* rise = "\033[1;36m";
        constexpr const char* fall = "\033[1;33m";
        constexpr const char* highZ = "\033[1;34m";
        constexpr const char* error = "\033[1;31m";
        constexpr const char* busBorder = "\033[1;35m";
        constexpr const char* busValue = "\033[1;37m";
        constexpr const char* hierarchy = "\033[1;34m";
        constexpr const char* treeWhite = "\033[1;37m";
        constexpr const char* input = "\033[1;36m";
        constexpr const char* output = "\033[1;33m";
        constexpr const char* internal = "\033[2;37m";

        struct Size
        {
            size_t columns = 80;
            size_t rows = 24;
        };

        enum class Key { none, resize, left, right, up, down, quit };

        struct Row
        {
            std::string prefix;
            std::string name;
            const Wave* wave;
        };

        // The terminal counts Unicode code points as columns, while std::string
        // counts bytes. These helpers keep tree glyphs aligned with ASCII text.
        size_t utf8Width(const std::string& text)
        {
            size_t width = 0;
            for (size_t i = 0; i < text.size(); ++i)
            {
                if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) ++width;
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

        LogicVector valueAt(const std::vector<Sample>& samples, uint64_t time)
        {
            const auto it = std::upper_bound(
                samples.begin(), samples.end(), time,
                [](uint64_t value, const Sample& sample) {
                    return value < sample.timestamp;
                });

            if (it == samples.begin())
            {
                return LogicVector::HighZ();
            }

            return std::prev(it)->value;
        }

        // Flatten the hierarchy into display rows while preserving the tree
        // guides needed by the terminal view.
        void collectRows(const Waveform& waveform, const std::string& ancestor, std::vector<Row>& rows)
        {
            std::vector<std::pair<std::string, const Wave*>> signals;
            for (const auto& [name, wave] : waveform.signals)
            {
                signals.emplace_back(name, &wave);
            }
            std::sort(signals.begin(), signals.end());
            std::vector<std::pair<std::string, const Waveform*>> graphs;
            for (const auto& [name, graph] : waveform.subgraphs)
            {
                graphs.emplace_back(name, &graph);
            }
            std::sort(graphs.begin(), graphs.end());
            const size_t children = signals.size() + graphs.size();
            size_t index = 0;
            for (const auto& [name, wave] : signals)
            {
                const bool isLast = ++index == children;
                rows.push_back({ ancestor + (isLast ? "└─ " : "├─ "), name, wave });
            }
            for (const auto& [name, graph] : graphs)
            {
                const bool isLast = ++index == children;
                const std::string prefix = ancestor + (isLast ? "└─ " : "├─ ");
                rows.push_back({ prefix, "▼ " + name, nullptr });
                collectRows(*graph, ancestor + (isLast ? "   " : "│  "), rows);
            }
        }

        // Append only transitions; repeated snapshots do not grow the trace.
        void recordSnapshot(Waveform& waveform, const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp)
        {
            const auto recordSignals = [&waveform, timestamp](const auto& signals) {
                for (const auto& [name, state] : signals)
                {
                    auto it = waveform.signals.find(name);
                    if (it != waveform.signals.end() && !it->second.samples.empty() && it->second.samples.back().value != state.second)
                        it->second.samples.push_back({ state.second, timestamp });
                }
                };
            recordSignals(snapshot.inputs); recordSignals(snapshot.outputs); recordSignals(snapshot.wires);
            for (const auto& [name, child] : snapshot.subgraphs)
                if (auto it = waveform.subgraphs.find(name); it != waveform.subgraphs.end()) recordSnapshot(it->second, child, timestamp);
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

        std::string cursorValue(const Wave& wave, uint64_t time)
        {
            const LogicVector value = valueAt(wave.samples, time);
            if (wave.width == 1)
            {
                return std::string(1, value.bit(0));
            }

            return formatBusValue(value, wave.width);
        }

        // Render one signal row. Single-bit signals use level glyphs; buses
        // divide their samples into runs so values remain readable.
        std::string renderWave(const Wave& wave, uint64_t start, uint64_t end, uint64_t cursor)
        {
            std::ostringstream out;
            if (wave.width == 1)
            {
                char previous = valueAt(wave.samples, start).bit(0);
                for (uint64_t time = start; time < end; ++time)
                {
                    const char current = valueAt(wave.samples, time).bit(0);
                    if (time == cursor)
                    {
                        out << error << "│";
                    }
                    else if (current == '1')
                    {
                        out << (previous == '0' ? rise : high)
                            << (previous == '0' ? "/" : "‾");
                    }
                    else if (current == '0')
                    {
                        out << (previous == '1' ? fall : low)
                            << (previous == '1' ? '\\' : '_');
                    }
                    else if (current == 'Z')
                    {
                        out << highZ << '.';
                    }
                    else
                    {
                        out << error << '?';
                    }
                    out << reset;
                    previous = current;
                }
                return out.str();
            }

            for (uint64_t time = start; time < end;)
            {
                const LogicVector value = valueAt(wave.samples, time);
                uint64_t run = 1;
                while (time + run < end && valueAt(wave.samples, time + run) == value)
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
                if (cursorOffset < cells.size())
                {
                    cells[cursorOffset] = '\x01';
                }

                for (size_t i = 0; i < cells.size(); ++i)
                {
                    const bool cursorCell = i == cursorOffset;
                    const bool borderCell = i == 0;
                    const bool errorCell = !cursorCell && formatBusValue(value, wave.width) == "error";
                    const char* color = cursorCell
                        ? error
                        : (errorCell ? error : (borderCell ? busBorder : busValue));
                    const std::string glyph = cells[i] == '\x01'
                        ? "│"
                        : std::string(1, cells[i]);
                    out << color << glyph << reset;
                }
                time += run;
            }
            return out.str();
        }

        std::string renderCursor(uint64_t start, uint64_t end, uint64_t cursor)
        {
            std::string result;
            for (uint64_t time = start; time < end; ++time)
                result += time == cursor ? std::string(error) + "│" + reset : " ";
            return result;
        }

        // Query the dimensions of the active terminal, with an 80x24 fallback
        // for terminals that do not report a size.
        Size terminalSize()
        {
            #ifdef _WIN32
            CONSOLE_SCREEN_BUFFER_INFO info{};
            if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
            {
                return {
                    static_cast<size_t>(info.srWindow.Right - info.srWindow.Left + 1),
                    static_cast<size_t>(info.srWindow.Bottom - info.srWindow.Top + 1)
                };
            }
            #else
            winsize info{};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &info) == 0 && info.ws_col && info.ws_row)
            {
                return { info.ws_col, info.ws_row };
            }
            #endif
            return {};
        }

        bool interactive()
        {
            #ifdef _WIN32
            DWORD inputMode{}, outputMode{};
            return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &inputMode)
                && GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &outputMode);
            #else
            return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
            #endif
        }

        // Read one navigation event. Platform-specific implementations below
        // both block while idle and report terminal resize events.
        #ifdef _WIN32
        Key readKey()
        {
            HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
            for (;;)
            {
                INPUT_RECORD record{};
                DWORD count{};
                if (!ReadConsoleInput(input, &record, 1, &count))
                {
                    return Key::quit;
                }
                if (record.EventType == WINDOW_BUFFER_SIZE_EVENT)
                {
                    return Key::resize;
                }
                if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                {
                    continue;
                }

                const auto code = record.Event.KeyEvent.wVirtualKeyCode;
                if (code == VK_LEFT) return Key::left;
                if (code == VK_RIGHT) return Key::right;
                if (code == VK_UP) return Key::up;
                if (code == VK_DOWN) return Key::down;
                if (code == VK_ESCAPE
                    || record.Event.KeyEvent.uChar.UnicodeChar == L'q'
                    || record.Event.KeyEvent.uChar.UnicodeChar == L'Q')
                {
                    return Key::quit;
                }
            }
        }
        #else
        class RawTerminal
        {
            termios old{};

        public:
            RawTerminal()
            {
                tcgetattr(STDIN_FILENO, &old);
                auto raw = old;
                raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            }

            ~RawTerminal()
            {
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
            }
        };

        volatile std::sig_atomic_t terminalResized = 0;

        void onTerminalResize(int)
        {
            terminalResized = 1;
        }

        Key readKey()
        {
            fd_set input;
            FD_ZERO(&input);
            FD_SET(STDIN_FILENO, &input);

            if (select(STDIN_FILENO + 1, &input, nullptr, nullptr, nullptr) < 0)
            {
                if (terminalResized)
                {
                    terminalResized = 0;
                    return Key::resize;
                }
                return Key::none;
            }

            char key{};
            if (::read(STDIN_FILENO, &key, 1) != 1)
            {
                return Key::none;
            }
            if (key == 'q' || key == 'Q')
            {
                return Key::quit;
            }
            if (key != '\x1b')
            {
                return Key::none;
            }

            char sequence[2]{};
            if (::read(STDIN_FILENO, sequence, 2) != 2 || sequence[0] != '[')
            {
                return Key::quit;
            }

            switch (sequence[1])
            {
                case 'A': return Key::up;
                case 'B': return Key::down;
                case 'C': return Key::right;
                case 'D': return Key::left;
                default: return Key::none;
            }
        }
        #endif

        // Draw one complete frame. The metadata panel and timeline are kept
        // independent so the separator remains stable as values change.
        void draw(
            const std::vector<Row>& rows,
            uint64_t start,
            uint64_t end,
            uint64_t cursor,
            size_t firstRow,
            Size size,
            bool controls)
        {
            const size_t panelWidth = std::clamp(size.columns / 3, size_t{ 46 }, size_t{ 60 });
            size_t valueWidth = 3;
            for (const Row& row : rows)
            {
                if (row.wave)
                {
                    valueWidth = std::max(
                        valueWidth,
                        row.wave->width == 1
                        ? size_t{ 1 }
                    : std::min<size_t>(18, (row.wave->width + 3) / 4 + 2));
                }
            }

            const size_t typeWidth = 6;
            const size_t nameWidth = panelWidth > valueWidth + typeWidth + 2
                ? panelWidth - valueWidth - typeWidth - 2
                : 8;
            const size_t timeWidth = std::max<size_t>(
                1,
                size.columns > panelWidth + 3 ? size.columns - panelWidth - 3 : 1);
            const uint64_t visibleEnd = std::min(end, start + timeWidth);
            const size_t visibleRows = std::max<size_t>(1, size.rows > 3 ? size.rows - 3 : 1);

            std::cout << "\033[2K"
                << dim << fit("", nameWidth)
                << ' ' << fit("", typeWidth)
                << ' ' << rightFit(cursor == end ? "" : std::to_string(cursor), valueWidth - 2) << "fs"
                << " │ Time " << start << ".." << (visibleEnd - 1) << reset << '\n';
            for (size_t i = firstRow; i < rows.size() && i < firstRow + visibleRows; ++i)
            {
                const Row& row = rows[i];
                if (!row.wave)
                {
                    std::cout << "\033[2K"
                        << hierarchy << fit(row.prefix + row.name, nameWidth) << reset
                        << ' ' << fit("", typeWidth)
                        << ' ' << fit("", valueWidth)
                        << " │" << renderCursor(start, visibleEnd, cursor) << '\n';
                }
                else
                {
                    const std::string value = cursor < end ? cursorValue(*row.wave, cursor) : "";
                    std::cout << "\033[2K"
                        << treeWhite << fit(row.prefix + row.name, nameWidth)
                        << ' ' << dim << fit(typeName(row.wave->type), typeWidth)
                        << ' ' << rightFit(value, valueWidth) << reset
                        << " │" << renderWave(*row.wave, start, visibleEnd, cursor) << '\n';
                }
            }
            std::cout << "\033[2K" << dim
                << "Rows " << (rows.empty() ? 0 : firstRow + 1)
                << '-' << std::min(rows.size(), firstRow + visibleRows)
                << '/' << rows.size();
            if (controls)
            {
                std::cout << "  Cursor: " << cursor
                    << "  Arrows: move cursor/rows  Q/Esc: quit";
            }
            std::cout << reset << "\033[J\n";
        }
    }

    // Public waveform API ---------------------------------------------------

    WaveformRecorder::WaveformRecorder(const Engine::SubgraphSnapshot& snapshot)
    {
        const auto add = [this](const auto& signals, SignalType type) {
            for (const auto& [name, state] : signals)
            {
                m_waveform.signals.emplace(
                    name,
                    Wave{ state.first, type, {{ state.second, 0 }} });
            }
            };

        add(snapshot.inputs, SignalType::Input);
        add(snapshot.outputs, SignalType::Output);
        add(snapshot.wires, SignalType::Internal);

        for (const auto& [name, child] : snapshot.subgraphs)
        {
            m_waveform.subgraphs.emplace(
                name,
                WaveformRecorder(child).waveform());
        }
    }

    void WaveformRecorder::record(const Engine::SubgraphSnapshot& snapshot, uint64_t timestamp)
    {
        recordSnapshot(m_waveform, snapshot, timestamp);
    }

    // Format definite buses with width-derived hexadecimal padding. Undefined
    // logic is intentionally kept visible as the red "error" marker.
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

    // Launch the interactive viewer when a TTY is available; otherwise emit a
    // single static frame for scripts and redirected output.
    void showWaveform(const Waveform& waveform, uint64_t startTime, uint64_t endTime)
    {
        if (startTime >= endTime)
        {
            return;
        }

        std::vector<Row> rows;
        collectRows(waveform, "", rows);
        if (!interactive())
        {
            draw(rows, startTime, endTime, startTime, 0, terminalSize(), false);
            return;
        }

        #ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        DWORD outputMode{};
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(output, &outputMode);
        SetConsoleMode(output, outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        #else
        RawTerminal raw;
        struct sigaction action { };
        action.sa_handler = onTerminalResize;
        sigemptyset(&action.sa_mask);
        struct sigaction oldAction { };
        sigaction(SIGWINCH, &action, &oldAction);
        #endif

        std::cout << "\033[?1049h\033[H" << std::flush;
        uint64_t time = startTime;
        uint64_t cursor = startTime;
        size_t firstRow = 0;
        bool running = true;
        bool redraw = true;
        size_t timeWidth = 1;
        size_t rowCount = 1;
        uint64_t lastTime = startTime;

        while (running)
        {
            if (redraw)
            {
                const Size size = terminalSize();
                const size_t panelWidth = std::clamp(size.columns / 3, size_t{ 46 }, size_t{ 60 });
                timeWidth = std::max<size_t>(
                    1,
                    size.columns > panelWidth + 3 ? size.columns - panelWidth - 3 : 1);
                rowCount = std::max<size_t>(1, size.rows > 3 ? size.rows - 3 : 1);
                lastTime = endTime > timeWidth ? endTime - timeWidth : startTime;
                cursor = std::clamp(cursor, startTime, endTime - 1);
                if (cursor < time)
                {
                    time = cursor;
                }
                if (cursor >= time + timeWidth)
                {
                    time = cursor - timeWidth + 1;
                }
                time = std::clamp(time, startTime, lastTime);
                firstRow = rows.size() > rowCount
                    ? std::min(firstRow, rows.size() - rowCount)
                    : 0;

                std::cout << "\033[H";
                draw(rows, time, endTime, cursor, firstRow, size, true);
                std::cout.flush();
                redraw = false;
            }

            const Key key = readKey();
            redraw = key == Key::resize;

            switch (key)
            {
                case Key::left:
                    if (cursor > startTime)
                    {
                        --cursor;
                        redraw = true;
                    }
                    break;
                case Key::right:
                    if (cursor + 1 < endTime)
                    {
                        ++cursor;
                        redraw = true;
                    }
                    break;
                case Key::up:
                    if (firstRow > 0)
                    {
                        --firstRow;
                        redraw = true;
                    }
                    break;
                case Key::down:
                    if (firstRow + rowCount < rows.size())
                    {
                        ++firstRow;
                        redraw = true;
                    }
                    break;
                case Key::quit:
                    running = false;
                    break;
                default:
                    break;
            }
        }

        std::cout << "\033[0m\033[?1049l" << std::flush;
        #ifdef _WIN32
        SetConsoleMode(output, outputMode);
        #else
        sigaction(SIGWINCH, &oldAction, nullptr);
        #endif
    }
}
