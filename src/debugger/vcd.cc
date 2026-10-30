#include "vcd.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "checked_math.h"

namespace Pulse::Debugger
{
    namespace
    {
        // ----------------------------------------------------------------------------------------
        // Shared helpers
        // ----------------------------------------------------------------------------------------

        constexpr const char* kSignalComment = "pulse-signal";

        std::string lowercase(std::string text)
        {
            for (char& c : text)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return text;
        }

        const char* formatName(Engine::DisplayFormat format)
        {
            switch (format)
            {
                case Engine::DisplayFormat::Bit: return "bit";
                case Engine::DisplayFormat::Boolean: return "boolean";
                case Engine::DisplayFormat::Hex: return "hex";
                case Engine::DisplayFormat::SignedInteger: return "signed";
                case Engine::DisplayFormat::UnsignedInteger: return "unsigned";
                case Engine::DisplayFormat::Enumeration: return "enum";
            }
            return "hex";
        }

        std::optional<Engine::DisplayFormat> formatNamed(const std::string& name)
        {
            static const std::pair<const char*, Engine::DisplayFormat> formats[] = {
                { "bit", Engine::DisplayFormat::Bit },
                { "boolean", Engine::DisplayFormat::Boolean },
                { "hex", Engine::DisplayFormat::Hex },
                { "signed", Engine::DisplayFormat::SignedInteger },
                { "unsigned", Engine::DisplayFormat::UnsignedInteger },
                { "enum", Engine::DisplayFormat::Enumeration },
            };
            for (const auto& [text, format] : formats)
                if (name == text)
                    return format;
            return std::nullopt;
        }

        const char* directionName(SignalType type)
        {
            switch (type)
            {
                case SignalType::Input: return "input";
                case SignalType::Output: return "output";
                case SignalType::Internal: return "internal";
            }
            return "internal";
        }

        std::optional<SignalType> directionNamed(const std::string& name)
        {
            if (name == "input") return SignalType::Input;
            if (name == "output") return SignalType::Output;
            if (name == "internal") return SignalType::Internal;
            return std::nullopt;
        }

        /// An enumeration literal as one token of a comment: blanks, '%' and non-ASCII bytes become %XX.
        std::string encodeLiteral(const std::string& literal)
        {
            std::string text;
            for (const char c : literal)
            {
                const auto byte = static_cast<unsigned char>(c);
                if (byte <= ' ' || byte == '%' || byte > '~')
                {
                    char escaped[4];
                    std::snprintf(escaped, sizeof escaped, "%%%02X", byte);
                    text += escaped;
                }
                else
                    text.push_back(c);
            }
            return text.empty() ? "%" : text;    // never an empty token (a lone '%' decodes to "")
        }

        std::string decodeLiteral(const std::string& text)
        {
            std::string literal;
            for (size_t i = 0; i < text.size(); ++i)
            {
                unsigned value = 0;
                if (text[i] == '%' && i + 2 < text.size()
                    && std::from_chars(text.data() + i + 1, text.data() + i + 3, value, 16).ptr == text.data() + i + 3)
                {
                    literal.push_back(static_cast<char>(value));
                    i += 2;
                }
                else if (text[i] != '%')
                    literal.push_back(text[i]);
            }
            return literal;
        }

        bitWidth_t clampedWidth(bitWidth_t width)
        {
            return std::clamp<bitWidth_t>(width, 1, BITWIDTH_MAX);
        }

        // ----------------------------------------------------------------------------------------
        // Writer
        // ----------------------------------------------------------------------------------------

        /// The identifier code of the variable at `index`: base 94 over the printable characters '!' to '~'.
        std::string identifierCode(size_t index)
        {
            std::string code;
            do
            {
                code.push_back(static_cast<char>('!' + index % 94));
                index /= 94;
            } while (index > 0);
            return code;
        }

        struct WrittenVariable
        {
            const Wave* wave;
            std::string id;
        };

        template <typename Map>
        std::vector<std::string> sortedKeys(const Map& map)
        {
            std::vector<std::string> keys;
            keys.reserve(map.size());
            for (const auto& entry : map)
                keys.push_back(entry.first);
            std::sort(keys.begin(), keys.end());
            return keys;
        }

        void declareScope(std::ostream& out, const WaveformData& data, const std::string& name,
                          std::vector<WrittenVariable>& variables)
        {
            out << "$scope module " << name << " $end\n";

            for (const std::string& signal : sortedKeys(data.signals))
            {
                const Wave& wave = data.signals.at(signal);
                const bitWidth_t width = clampedWidth(wave.width);
                const std::string id = identifierCode(variables.size());
                const bool integer = wave.display.format == Engine::DisplayFormat::SignedInteger
                                  || wave.display.format == Engine::DisplayFormat::UnsignedInteger;

                out << "$var " << (integer ? "integer" : "wire") << ' ' << static_cast<int>(width) << ' ' << id << ' '
                    << signal;
                if (wave.display.format == Engine::DisplayFormat::Hex)
                    out << " [" << wave.display.indexOfBit(width - 1, width) << ':' << wave.display.indexOfBit(0, width) << ']';
                out << " $end\n";

                out << "$comment " << kSignalComment << ' ' << id << ' ' << directionName(wave.type) << ' '
                    << formatName(wave.display.format);
                if (wave.display.format == Engine::DisplayFormat::Enumeration)
                    for (const std::string& literal : wave.display.literals)
                        out << ' ' << encodeLiteral(literal);
                out << " $end\n";

                variables.push_back({ &wave, id });
            }

            for (const std::string& child : sortedKeys(data.subgraphs))
                declareScope(out, data.subgraphs.at(child), child, variables);

            out << "$upscope $end\n";
        }

        /// A value change: `0!` for one bit, `b0101 "` for a vector, with 'x' and 'z' in lowercase.
        void writeValue(std::ostream& out, const Engine::LogicVector& value, bitWidth_t width, const std::string& id)
        {
            const std::string bits = lowercase(value.range(width).str(width));
            if (width == 1)
                out << bits << id << '\n';
            else
                out << 'b' << bits << ' ' << id << '\n';
        }

        // ----------------------------------------------------------------------------------------
        // Reader
        // ----------------------------------------------------------------------------------------

        /// Splits a VCD file into its whitespace-separated tokens, keeping the line of each one for errors.
        class Lexer
        {
            std::streambuf* m_buffer;
            size_t m_line = 1;
            size_t m_tokenLine = 1;

        public:
            explicit Lexer(std::istream& in) : m_buffer(in.rdbuf()) { }

            bool next(std::string& token)
            {
                token.clear();
                if (!m_buffer)
                    return false;

                int c = m_buffer->sbumpc();
                while (c != EOF && std::isspace(c))
                {
                    if (c == '\n')
                        ++m_line;
                    c = m_buffer->sbumpc();
                }
                if (c == EOF)
                    return false;

                m_tokenLine = m_line;
                token.push_back(static_cast<char>(c));
                while ((c = m_buffer->sgetc()) != EOF && !std::isspace(c))
                    token.push_back(static_cast<char>(m_buffer->sbumpc()));
                return true;
            }

            /// The tokens of a command, up to its `$end`.
            std::vector<std::string> untilEnd(const std::string& command)
            {
                std::vector<std::string> tokens;
                std::string token;
                const size_t line = m_tokenLine;
                while (next(token))
                {
                    if (token == "$end")
                        return tokens;
                    tokens.push_back(token);
                }
                throw vcd_error(line, "'" + command + "' has no '$end'");
            }

            [[noreturn]] void fail(const std::string& message) const
            {
                throw vcd_error(m_tokenLine, message);
            }
        };

        struct ReadVariable
        {
            Wave* wave;
            std::string path;   ///< Dotted path, for error messages
        };

        /// An unsigned decimal number, or nullopt when the text is not one or does not fit in an int64.
        std::optional<int64_t> decimal(const std::string& text)
        {
            int64_t number = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
            if (text.empty() || error != std::errc{} || end != text.data() + text.size() || number < 0)
                return std::nullopt;
            return number;
        }

        class Reader
        {
            Lexer m_lexer;
            WaveformData m_top;                     ///< Above the root: the top-level scopes and variables
            std::vector<WaveformData*> m_scopes;    ///< The open scopes, innermost last
            std::vector<std::string> m_scopeNames;
            std::unordered_map<std::string, std::vector<ReadVariable>> m_variables;    ///< By identifier code
            std::unordered_set<std::string> m_skippedIds;
            int64_t m_timescale = 1;                ///< Femtoseconds per time unit of the file
            simTime_t m_now = 0;
            VcdTrace m_trace;

        public:
            explicit Reader(std::istream& in) : m_lexer(in), m_scopes{ &m_top } { }

            VcdTrace read()
            {
                readDeclarations();
                readValueChanges();

                // A variable without any value is unknown all along.
                for (auto& [id, variables] : m_variables)
                    for (ReadVariable& variable : variables)
                        if (variable.wave->samples.empty())
                            variable.wave->samples.push_back({ unknown(*variable.wave), 0 });

                // The only top-level scope is the root; otherwise everything hangs from a "root".
                if (m_top.signals.empty() && m_top.subgraphs.size() == 1)
                {
                    auto root = m_top.subgraphs.extract(m_top.subgraphs.begin());
                    m_trace.rootName = root.key();
                    m_trace.waveform = std::move(root.mapped());
                }
                else
                    m_trace.waveform = std::move(m_top);
                return std::move(m_trace);
            }

        private:
            static Engine::LogicVector unknown(const Wave& wave)
            {
                return Engine::LogicVector::Unknown().range(clampedWidth(wave.width));
            }

            std::string pathOf(const std::string& name) const
            {
                std::string path;
                for (const std::string& scope : m_scopeNames)
                    path += scope + '.';
                return path + name;
            }

            // ---- Declarations -------------------------------------------------------------------

            void readDeclarations()
            {
                std::string token;
                while (m_lexer.next(token))
                {
                    if (token == "$enddefinitions")
                    {
                        m_lexer.untilEnd(token);
                        return;
                    }
                    if (token == "$scope")
                        openScope(m_lexer.untilEnd(token));
                    else if (token == "$upscope")
                    {
                        m_lexer.untilEnd(token);
                        if (m_scopes.size() == 1)
                            m_lexer.fail("'$upscope' without an open '$scope'");
                        m_scopes.pop_back();
                        m_scopeNames.pop_back();
                    }
                    else if (token == "$var")
                        declareVariable(m_lexer.untilEnd(token));
                    else if (token == "$timescale")
                        setTimescale(m_lexer.untilEnd(token));
                    else if (token == "$comment")
                        readComment(m_lexer.untilEnd(token));
                    else if (token.front() == '$')
                        m_lexer.untilEnd(token);    // $date, $version and the commands of other tools
                    else
                        m_lexer.fail("unexpected '" + token + "' in the declarations of the file");
                }
                m_lexer.fail("the file ends before '$enddefinitions'");
            }

            void openScope(const std::vector<std::string>& arguments)
            {
                if (arguments.size() != 2)
                    m_lexer.fail("expected '$scope <type> <name> $end'");
                const std::string& name = arguments[1];
                m_scopes.push_back(&m_scopes.back()->subgraphs[name]);
                m_scopeNames.push_back(name);
            }

            void setTimescale(const std::vector<std::string>& arguments)
            {
                std::string text;
                for (const std::string& argument : arguments)
                    text += argument;

                const size_t unitStart = text.find_first_not_of("0123456789");
                const std::optional<int64_t> number = decimal(text.substr(0, unitStart));
                const std::string unit = unitStart == std::string::npos ? "" : lowercase(text.substr(unitStart));

                static const std::pair<const char*, int64_t> units[] = {
                    { "fs", 1 }, { "ps", 1'000 }, { "ns", 1'000'000 }, { "us", 1'000'000'000 },
                    { "ms", 1'000'000'000'000 }, { "s", 1'000'000'000'000'000 },
                };
                const auto found = std::find_if(std::begin(units), std::end(units),
                                                [&](const auto& entry) { return unit == entry.first; });

                std::optional<int64_t> scale;
                if (number && *number > 0 && found != std::end(units))
                    scale = checkedMul(*number, found->second);
                if (!scale)
                    m_lexer.fail("invalid '$timescale " + text + "': expected a number followed by s, ms, us, ns, ps or fs");
                m_timescale = *scale;
            }

            void declareVariable(const std::vector<std::string>& arguments)
            {
                if (arguments.size() < 4)
                    m_lexer.fail("expected '$var <type> <size> <identifier> <name> $end'");

                const std::string type = lowercase(arguments[0]);
                const std::optional<int64_t> size = decimal(arguments[1]);
                const std::string& id = arguments[2];
                if (!size || *size == 0)
                    m_lexer.fail("invalid size '" + arguments[1] + "' of a variable");

                // The name may carry a range, apart ("count [7:0]") or not ("count[7:0]").
                std::string reference;
                for (size_t i = 3; i < arguments.size(); ++i)
                    reference += arguments[i];

                std::string name = reference;
                std::optional<std::pair<int64_t, int64_t>> range;
                const size_t open = reference.find('[');
                if (open != std::string::npos && open > 0 && reference.back() == ']')
                {
                    const std::string inside = reference.substr(open + 1, reference.size() - open - 2);
                    const size_t colon = inside.find(':');
                    if (colon != std::string::npos)
                    {
                        const auto bound = [](const std::string& text) -> std::optional<int64_t>
                        {
                            int64_t value = 0;
                            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
                            if (text.empty() || error != std::errc{} || end != text.data() + text.size())
                                return std::nullopt;
                            return value;
                        };
                        const auto left = bound(inside.substr(0, colon));
                        const auto right = bound(inside.substr(colon + 1));
                        if (left && right)
                        {
                            name = reference.substr(0, open);
                            range = { *left, *right };
                        }
                    }
                    // A single index ("data[3]") selects one bit of a vector: it stays part of the name.
                }

                const std::string path = pathOf(name);
                m_skippedIds.insert(id);
                if (type == "real" || type == "realtime" || type == "shortreal")
                    return skip("'" + path + "' is a real variable");
                if (type == "string")
                    return skip("'" + path + "' is a string variable");
                if (*size > BITWIDTH_MAX)
                    return skip("'" + path + "' is " + std::to_string(*size) + " bits wide (Pulse shows up to "
                                + std::to_string(BITWIDTH_MAX) + ")");

                WaveformData& scope = *m_scopes.back();
                const auto [entry, inserted] = scope.signals.try_emplace(name);
                if (!inserted)
                    return skip("'" + path + "' is declared twice; only its first declaration is shown");

                Wave& wave = entry->second;
                wave.width = static_cast<bitWidth_t>(*size);
                wave.type = SignalType::Internal;
                if (type == "integer")
                    wave.display.format = Engine::DisplayFormat::SignedInteger;
                else
                    wave.display.format = wave.width == 1 ? Engine::DisplayFormat::Bit : Engine::DisplayFormat::Hex;

                const int64_t width = wave.width;
                if (range && (range->first - range->second == width - 1 || range->second - range->first == width - 1))
                {
                    wave.display.left = range->first;
                    wave.display.ascending = range->first < range->second;
                }
                else
                {
                    wave.display.left = width - 1;
                    wave.display.ascending = false;
                }

                m_variables[id].push_back({ &wave, path });
            }

            void skip(std::string reason)
            {
                m_trace.skipped.push_back(std::move(reason));
            }

            void readComment(const std::vector<std::string>& arguments)
            {
                if (arguments.empty() || arguments[0] != kSignalComment)
                    return;

                // pulse-signal <id> <direction> <format> [literals...]
                const auto direction = arguments.size() >= 4 ? directionNamed(arguments[2]) : std::nullopt;
                const auto format = arguments.size() >= 4 ? formatNamed(arguments[3]) : std::nullopt;
                if (!direction || !format)
                    m_lexer.fail("malformed '$comment " + std::string(kSignalComment) + "'");

                const auto variables = m_variables.find(arguments[1]);
                if (variables == m_variables.end())
                    return;     // a variable that was skipped
                for (ReadVariable& variable : variables->second)
                {
                    Wave& wave = *variable.wave;
                    wave.type = *direction;
                    wave.display.format = *format;
                    wave.display.literals.clear();
                    if (*format == Engine::DisplayFormat::Enumeration)
                        for (size_t i = 4; i < arguments.size(); ++i)
                            wave.display.literals.push_back(decodeLiteral(arguments[i]));
                }
            }

            // ---- Value changes ------------------------------------------------------------------

            void readValueChanges()
            {
                std::string token;
                std::string id;
                while (m_lexer.next(token))
                {
                    const char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(token.front())));
                    if (kind == '#')
                        setTime(token);
                    else if (kind == '$')
                    {
                        if (token == "$comment")
                            m_lexer.untilEnd(token);
                        else if (token != "$dumpvars" && token != "$dumpall" && token != "$dumpon" && token != "$dumpoff"
                                 && token != "$end")
                            m_lexer.fail("unexpected '" + token + "' after '$enddefinitions'");
                    }
                    else if (kind == 'b')
                    {
                        if (!m_lexer.next(id))
                            m_lexer.fail("the vector value '" + token + "' has no identifier code");
                        change(id, token.substr(1), token);
                    }
                    else if (kind == 'r' || kind == 's')
                    {
                        if (!m_lexer.next(id))
                            m_lexer.fail("the value '" + token + "' has no identifier code");
                        if (m_variables.count(id))
                            m_lexer.fail("the value '" + token + "' is not a logic value");
                        if (!m_skippedIds.count(id))
                            m_lexer.fail("unknown identifier code '" + id + "'");
                    }
                    else if (token.size() >= 2)
                        change(token.substr(1), token.substr(0, 1), token);
                    else
                        m_lexer.fail("unexpected '" + token + "'");
                }
            }

            void setTime(const std::string& token)
            {
                const std::optional<int64_t> units = decimal(token.substr(1));
                const std::optional<int64_t> time = units ? checkedMul(*units, m_timescale) : std::nullopt;
                if (!units)
                    m_lexer.fail("invalid timestamp '" + token + "'");
                if (!time)
                    m_lexer.fail("timestamp '" + token + "' is too large (Pulse keeps time in femtoseconds, up to about 2.5 hours)");
                const auto now = static_cast<simTime_t>(*time);
                if (now < m_now)
                    m_lexer.fail("timestamp '" + token + "' goes back in time");
                m_now = now;
                m_trace.endTime = std::max(m_trace.endTime, now);
            }

            /// The engine's value of one VCD bit.
            std::optional<Engine::LogicVector> bitValue(char c) const
            {
                switch (std::tolower(static_cast<unsigned char>(c)))
                {
                    case '0': case 'l': return Engine::LogicVector(0, 0);
                    case '1': case 'h': return Engine::LogicVector(1, 0);
                    case 'z': return Engine::LogicVector(1, 1);
                    case 'x': case 'u': case 'w': case '-': return Engine::LogicVector(0, 1);
                    default: return std::nullopt;
                }
            }

            void change(const std::string& id, const std::string& bits, const std::string& token)
            {
                const auto variables = m_variables.find(id);
                if (variables == m_variables.end())
                {
                    if (m_skippedIds.count(id))
                        return;
                    m_lexer.fail("unknown identifier code '" + id + "'");
                }
                if (bits.empty())
                    m_lexer.fail("the value '" + token + "' has no bits");

                for (ReadVariable& variable : variables->second)
                {
                    Wave& wave = *variable.wave;
                    if (bits.size() > wave.width)
                        m_lexer.fail("the value '" + token + "' has " + std::to_string(bits.size()) + " bits, but '"
                                     + variable.path + "' has " + std::to_string(wave.width));

                    Engine::LogicVector value(0, 0);
                    for (size_t position = 0; position < wave.width; ++position)
                    {
                        // Bits missing on the left repeat an x or a z, and are 0 otherwise (IEEE 1364 18.2.1).
                        std::optional<Engine::LogicVector> bit;
                        if (position < bits.size())
                            bit = bitValue(bits[bits.size() - 1 - position]);
                        else
                        {
                            bit = bitValue(bits.front());
                            if (bit && !bit->mask)
                                bit = Engine::LogicVector(0, 0);
                        }
                        if (!bit)
                            m_lexer.fail("invalid value '" + token + "': the bits of a value are 0, 1, x and z");
                        value.value |= bit->value << position;
                        value.mask |= bit->mask << position;
                    }
                    addSample(wave, value);
                }
            }

            void addSample(Wave& wave, const Engine::LogicVector& value)
            {
                std::vector<Sample>& samples = wave.samples;
                if (samples.empty() && m_now > 0)
                    samples.push_back({ unknown(wave), 0 });
                if (samples.empty())
                {
                    samples.push_back({ value, m_now });
                    return;
                }

                // Several values at one time (delta cycles): the last one wins.
                if (samples.back().timestamp == m_now)
                {
                    samples.back().value = value;
                    if (samples.size() >= 2 && samples[samples.size() - 2].value == value)
                        samples.pop_back();
                    return;
                }
                if (samples.back().value != value)
                    samples.push_back({ value, m_now });
            }
        };
    } // anonymous namespace

    // --------------------------------------------------------------------------------------------
    // Public interface
    // --------------------------------------------------------------------------------------------

    void writeVcd(std::ostream& out, const WaveformData& waveform, const std::string& rootName, simTime_t endTime)
    {
        out << "$version Pulse $end\n";
        out << "$timescale 1 fs $end\n";

        std::vector<WrittenVariable> variables;
        declareScope(out, waveform, rootName.empty() ? "root" : rootName, variables);
        out << "$enddefinitions $end\n";

        // The values at time 0, then every transition by time.
        out << "#0\n$dumpvars\n";
        std::map<simTime_t, std::vector<std::pair<size_t, Engine::LogicVector>>> changes;
        for (size_t index = 0; index < variables.size(); ++index)
        {
            const Wave& wave = *variables[index].wave;
            const bitWidth_t width = clampedWidth(wave.width);
            const bool startsAtZero = !wave.samples.empty() && wave.samples.front().timestamp == 0;
            writeValue(out, startsAtZero ? wave.samples.front().value : Engine::LogicVector::Unknown(), width,
                       variables[index].id);

            for (size_t sample = startsAtZero ? 1 : 0; sample < wave.samples.size(); ++sample)
                changes[wave.samples[sample].timestamp].emplace_back(index, wave.samples[sample].value);
        }
        out << "$end\n";

        simTime_t last = 0;
        for (const auto& [time, values] : changes)
        {
            out << '#' << time << '\n';
            for (const auto& [index, value] : values)
                writeValue(out, value, clampedWidth(variables[index].wave->width), variables[index].id);
            last = time;
        }
        if (endTime > last)
            out << '#' << endTime << '\n';
    }

    VcdTrace readVcd(std::istream& in)
    {
        return Reader(in).read();
    }

    bool isVcdPath(const std::string& path)
    {
        const size_t dot = path.find_last_of('.');
        const size_t slash = path.find_last_of("/\\");
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
            return false;
        return lowercase(path.substr(dot)) == ".vcd";
    }
}
