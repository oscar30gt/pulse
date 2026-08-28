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
    struct Size
    {
        size_t columns = 80;
        size_t rows = 24;
    };

    enum class Key
    {
        none,
        resize,
        left,
        right,
        up,
        down,
        toggle,
        quit
    };

    struct Row
    {
        std::string prefix;
        std::string name;
        const Wave* wave = nullptr;
        std::string path;
        bool isGraph = false;
    };

    namespace Style
    {
        constexpr const char* reset = "\033[0m";
        constexpr const char* dim = "\033[2;37m";
        constexpr const char* high = "\033[1;32m";
        constexpr const char* low = "\033[2;37m";
        constexpr const char* highZ = "\033[1;34m";
        constexpr const char* error = "\033[1;31m";
        constexpr const char* cursorBg = "\033[41;1;37m";
        constexpr const char* focusBg = "\033[48;5;236m";
        constexpr const char* busBorder = "\033[1;35m";
        constexpr const char* busValue = "\033[1;37m";
        constexpr const char* hierarchy = "\033[1;34m";
        constexpr const char* treeWhite = "\033[1;37m";
        constexpr const char* input = "\033[1;36m";
        constexpr const char* output = "\033[1;33m";
        constexpr const char* internal = "\033[2;37m";
    }

    size_t utf8Width(const std::string& text);
    std::string truncateUtf8(const std::string& text, size_t width);
    std::string fit(std::string text, size_t width);
    std::string rightFit(std::string text, size_t width);
    std::string typeName(SignalType type);
    LogicVector valueAt(const std::vector<Sample>& samples, uint64_t time);
    std::string cursorValue(const Wave& wave, uint64_t time);

    void collectRows(
        const Waveform& waveform,
        const std::unordered_set<std::string>& expandedPaths,
        std::vector<Row>& rows);

    std::string renderWave(const Wave& wave, uint64_t start, uint64_t end, uint64_t cursor, bool focused);
    std::string renderCursor(uint64_t start, uint64_t end, uint64_t cursor, bool focused);

    std::vector<std::string> buildFrame(
        const std::vector<Row>& rows,
        uint64_t start,
        uint64_t end,
        uint64_t cursor,
        size_t firstRow,
        size_t selectedRow,
        Size size,
        bool controls);

    class TerminalRenderer
    {
        std::vector<std::string> m_frontBuffer;

    public:
        void render(const std::vector<std::string>& backBuffer);
        void clear();
    };

    Size terminalSize();
    bool interactive();
    Key readKey();
}

#endif // PULSE_WAVEFORM_INTERNAL_H
