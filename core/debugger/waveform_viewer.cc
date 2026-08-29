#include "waveform_internal.h"

#include <algorithm>
#include <iostream>

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
    // --------------------------------------------------------------------------------------------
    // Platform Terminal Configuration & Queries
    // --------------------------------------------------------------------------------------------

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

    // --------------------------------------------------------------------------------------------
    // Keyboard Input Event Loop
    // --------------------------------------------------------------------------------------------

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
            if (code == VK_RETURN || code == VK_SPACE) return Key::toggle;
            if (code == VK_ESCAPE
                || record.Event.KeyEvent.uChar.UnicodeChar == L'q'
                || record.Event.KeyEvent.uChar.UnicodeChar == L'Q')
            {
                return Key::quit;
            }
        }
    }
    #else
    /// RAII guard to enable and disable terminal raw mode on POSIX platforms.
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
        if (key == ' ' || key == '\r' || key == '\n')
        {
            return Key::toggle;
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

    // --------------------------------------------------------------------------------------------
    // Interactive Waveform Viewer Main Entry
    // --------------------------------------------------------------------------------------------

    void showWaveform(const Waveform& waveform, uint64_t startTime, uint64_t endTime)
    {
        if (startTime >= endTime)
        {
            return;
        }

        // Top-level "root" component is expanded by default on launch
        std::unordered_set<std::string> expandedPaths = { "root" };
        std::vector<Row> rows;
        collectRows(waveform, expandedPaths, rows);

        // Non-interactive fallback: render a single frame and write to standard output
        if (!interactive())
        {
            const auto frame = buildFrame(rows, startTime, endTime, startTime, 0, 0, terminalSize(), false);
            for (const auto& line : frame)
            {
                std::cout << line << '\n';
            }
            return;
        }

        // Terminal setup for interactive alternate screen buffer
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

        // Switch to alternate screen buffer (\033[?1049h), hide cursor (\033[?25l), clear (\033[2J\033[H)
        std::cout << "\033[?1049h\033[?25l\033[2J\033[H" << std::flush;

        uint64_t time = startTime;
        uint64_t cursor = startTime;
        size_t firstRow = 0;
        size_t selectedRow = 0;
        bool running = true;
        bool redraw = true;
        size_t timeWidth = 1;
        size_t rowCount = 1;
        uint64_t lastTime = startTime;
        TerminalRenderer renderer;

        // Interactive event loop
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

                // Auto-scroll timeline to keep cursor visible
                if (cursor < time)
                {
                    time = cursor;
                }
                if (cursor >= time + timeWidth)
                {
                    time = cursor - timeWidth + 1;
                }
                time = std::clamp(time, startTime, lastTime);

                // Auto-scroll row list to keep focused row visible
                selectedRow = std::min(selectedRow, rows.empty() ? 0 : rows.size() - 1);
                if (selectedRow < firstRow)
                {
                    firstRow = selectedRow;
                }
                else if (selectedRow >= firstRow + rowCount)
                {
                    firstRow = selectedRow - rowCount + 1;
                }

                firstRow = rows.size() > rowCount
                    ? std::min(firstRow, rows.size() - rowCount)
                    : 0;

                // Render back buffer differentially to screen
                renderer.render(buildFrame(rows, time, endTime, cursor, firstRow, selectedRow, size, true));
                redraw = false;
            }

            const Key key = readKey();

            // Handle terminal window resize
            if (key == Key::resize)
            {
                renderer.clear();
                std::cout << "\033[2J\033[H" << std::flush;
                redraw = true;
            }

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
                    if (selectedRow > 0)
                    {
                        --selectedRow;
                        if (selectedRow < firstRow)
                        {
                            firstRow = selectedRow;
                        }
                        redraw = true;
                    }
                    break;

                case Key::down:
                    if (selectedRow + 1 < rows.size())
                    {
                        ++selectedRow;
                        if (selectedRow >= firstRow + rowCount)
                        {
                            firstRow = selectedRow - rowCount + 1;
                        }
                        redraw = true;
                    }
                    break;

                case Key::toggle:
                    if (selectedRow < rows.size() && rows[selectedRow].isGraph)
                    {
                        const std::string& path = rows[selectedRow].path;
                        if (expandedPaths.find(path) != expandedPaths.end())
                        {
                            expandedPaths.erase(path);
                        }
                        else
                        {
                            expandedPaths.insert(path);
                        }

                        // Re-collect rows with new expansion state
                        rows.clear();
                        collectRows(waveform, expandedPaths, rows);

                        // Clamp selection and adjust viewport
                        selectedRow = std::min(selectedRow, rows.empty() ? 0 : rows.size() - 1);
                        if (selectedRow < firstRow)
                        {
                            firstRow = selectedRow;
                        }
                        else if (selectedRow >= firstRow + rowCount)
                        {
                            firstRow = selectedRow - rowCount + 1;
                        }
                        firstRow = rows.size() > rowCount
                            ? std::min(firstRow, rows.size() - rowCount)
                            : 0;

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

        // Restore terminal cursor (\033[?25h), reset styles, and exit alternate screen buffer (\033[?1049l)
        std::cout << "\033[?25h\033[0m\033[?1049l" << std::flush;

        #ifdef _WIN32
        SetConsoleMode(output, outputMode);
        #else
        sigaction(SIGWINCH, &oldAction, nullptr);
        #endif
    }
}
