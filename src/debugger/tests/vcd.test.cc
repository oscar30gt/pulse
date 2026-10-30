// vcd.test.cc — writing waveforms as Value Change Dump files and reading them back: the exact text Pulse writes, a
// lossless round trip (also of a simulated VHDL design), VCD files from other tools, the variables Pulse skips, and
// the errors of malformed files.

#include <gtest/gtest.h>

#include <sstream>

#include "test_helpers.h"
#include "vcd.h"
#include "waveform.h"

using namespace Pulse;
using namespace Pulse::Engine;
using namespace Pulse::Debugger;

namespace
{
    Wave waveOf(SignalType type, DisplayFormat format, bitWidth_t width, std::vector<Sample> samples)
    {
        Wave wave;
        wave.type = type;
        wave.width = width;
        wave.display.format = format;
        wave.display.left = width - 1;
        wave.samples = std::move(samples);
        return wave;
    }

    std::string written(const WaveformData& waveform, const std::string& root, simTime_t end)
    {
        std::ostringstream out;
        writeVcd(out, waveform, root, end);
        return out.str();
    }

    VcdTrace readText(const std::string& text)
    {
        std::istringstream in(text);
        return readVcd(in);
    }

    /// The error of reading a file, as "<line>: <message>".
    std::string readError(const std::string& text)
    {
        try
        {
            readText(text);
        }
        catch (const vcd_error& e)
        {
            return std::to_string(e.line()) + ": " + e.what();
        }
        return "<no error>";
    }

    /// The transitions of a wave, as the viewer sees them: values cut to the width of the wave, without repeats.
    std::vector<Sample> transitions(const Wave& wave)
    {
        std::vector<Sample> result;
        for (const Sample& sample : wave.samples)
        {
            const LogicVector value = sample.value.range(wave.width);
            if (result.empty() || result.back().value != value)
                result.push_back({ value, sample.timestamp });
        }
        return result;
    }

    /// Checks that two waveforms show the same thing: hierarchy, names, widths, directions, formats and transitions.
    void expectSameWaveform(const WaveformData& expected, const WaveformData& actual, const std::string& path)
    {
        ASSERT_EQ(expected.signals.size(), actual.signals.size()) << path;
        for (const auto& [name, wave] : expected.signals)
        {
            const auto found = actual.signals.find(name);
            ASSERT_NE(found, actual.signals.end()) << path << '.' << name;
            const Wave& other = found->second;
            EXPECT_EQ(wave.width, other.width) << path << '.' << name;
            EXPECT_EQ(wave.type, other.type) << path << '.' << name;
            EXPECT_EQ(wave.display.format, other.display.format) << path << '.' << name;
            EXPECT_EQ(wave.display.literals, other.display.literals) << path << '.' << name;
            if (wave.display.format == DisplayFormat::Hex)
            {
                EXPECT_EQ(wave.display.left, other.display.left) << path << '.' << name;
                EXPECT_EQ(wave.display.ascending, other.display.ascending) << path << '.' << name;
            }

            const std::vector<Sample> a = transitions(wave);
            const std::vector<Sample> b = transitions(other);
            ASSERT_EQ(a.size(), b.size()) << path << '.' << name;
            for (size_t i = 0; i < a.size(); ++i)
            {
                EXPECT_EQ(a[i].timestamp, b[i].timestamp) << path << '.' << name << " sample " << i;
                EXPECT_EQ(a[i].value, b[i].value) << path << '.' << name << " sample " << i;
            }
        }

        ASSERT_EQ(expected.subgraphs.size(), actual.subgraphs.size()) << path;
        for (const auto& [name, child] : expected.subgraphs)
        {
            const auto found = actual.subgraphs.find(name);
            ASSERT_NE(found, actual.subgraphs.end()) << path << '.' << name;
            expectSameWaveform(child, found->second, path + '.' + name);
        }
    }

    WaveformData smallDesign()
    {
        WaveformData data;
        data.signals["clk"] = waveOf(SignalType::Input, DisplayFormat::Bit, 1,
                                     { { LogicVector(0), 0 }, { LogicVector(1), 5 }, { LogicVector(0), 10 } });
        data.signals["count"] = waveOf(SignalType::Output, DisplayFormat::Hex, 4,
                                       { { LogicVector(0), 0 }, { LogicVector(0b0101), 5 }, { LogicVector(0b1001, 0b1100), 10 } });

        Wave state = waveOf(SignalType::Internal, DisplayFormat::Enumeration, 2, { { LogicVector(0), 0 }, { LogicVector(1), 10 } });
        state.display.literals = { "idle", "run" };
        data.subgraphs["u1"].signals["state"] = state;
        return data;
    }
}

// ---- Writing --------------------------------------------------------------------------------------

TEST(VcdWrite, TheHierarchyValuesAndEndTimeOfAWaveform)
{
    EXPECT_EQ(written(smallDesign(), "top", 20),
              "$version Pulse $end\n"
              "$timescale 1 fs $end\n"
              "$scope module top $end\n"
              "$var wire 1 ! clk $end\n"
              "$comment pulse-signal ! input bit $end\n"
              "$var wire 4 \" count [3:0] $end\n"
              "$comment pulse-signal \" output hex $end\n"
              "$scope module u1 $end\n"
              "$var wire 2 # state $end\n"
              "$comment pulse-signal # internal enum idle run $end\n"
              "$upscope $end\n"
              "$upscope $end\n"
              "$enddefinitions $end\n"
              "#0\n"
              "$dumpvars\n"
              "0!\n"
              "b0000 \"\n"
              "b00 #\n"
              "$end\n"
              "#5\n"
              "1!\n"
              "b0101 \"\n"
              "#10\n"
              "0!\n"
              "bzx01 \"\n"
              "b01 #\n"
              "#20\n");
}

TEST(VcdWrite, NoEndTimestampWhenTheLastChangeIsAtTheEnd)
{
    const std::string text = written(smallDesign(), "top", 10);
    const std::string tail = "#10\n0!\nbzx01 \"\nb01 #\n";
    ASSERT_GE(text.size(), tail.size());
    EXPECT_EQ(text.substr(text.size() - tail.size()), tail);
}

TEST(VcdWrite, IntegersAreIntegerVariablesAndVectorsKeepTheirRange)
{
    WaveformData data;
    data.signals["n"] = waveOf(SignalType::Internal, DisplayFormat::SignedInteger, 32, { { LogicVector(5), 0 } });
    Wave up = waveOf(SignalType::Internal, DisplayFormat::Hex, 4, { { LogicVector(1), 0 } });
    up.display.left = 0;
    up.display.ascending = true;
    data.signals["up"] = up;

    const std::string text = written(data, "top", 0);
    EXPECT_NE(text.find("$var integer 32 ! n $end"), std::string::npos) << text;
    EXPECT_NE(text.find("$var wire 4 \" up [0:3] $end"), std::string::npos) << text;
}

TEST(VcdWrite, IdentifierCodesStayUniqueBeyondOneCharacter)
{
    WaveformData data;
    for (int i = 0; i < 200; ++i)
        data.signals["s" + std::to_string(1000 + i)] = waveOf(SignalType::Internal, DisplayFormat::Bit, 1, { { LogicVector(i % 2), 0 } });

    const VcdTrace trace = readText(written(data, "top", 1));
    ASSERT_EQ(trace.waveform.signals.size(), 200u);
    for (int i = 0; i < 200; ++i)
        EXPECT_EQ(trace.waveform.signals.at("s" + std::to_string(1000 + i)).valueAt(0), LogicVector(i % 2));
}

// ---- Round trip -----------------------------------------------------------------------------------

TEST(VcdRoundTrip, AWaveformReadsBackAsItWasWritten)
{
    WaveformData data = smallDesign();
    Wave flag = waveOf(SignalType::Internal, DisplayFormat::Boolean, 1, { { LogicVector(0), 0 }, { LogicVector(1), 3 } });
    data.signals["flag"] = flag;
    data.signals["n"] = waveOf(SignalType::Internal, DisplayFormat::SignedInteger, 32,
                               { { LogicVector(0xFFFFFFF9), 0 }, { LogicVector(0, 0xFFFFFFFF), 4 } });
    data.signals["u"] = waveOf(SignalType::Internal, DisplayFormat::UnsignedInteger, 8, { { LogicVector(200), 0 } });
    data.signals["wide"] = waveOf(SignalType::Internal, DisplayFormat::Hex, 64, { { LogicVector(~0ULL), 0 }, { LogicVector(1ULL << 63), 7 } });
    Wave up = waveOf(SignalType::Output, DisplayFormat::Hex, 3, { { LogicVector(0b110), 0 } });
    up.display.left = 2;
    up.display.ascending = true;
    data.signals["up"] = up;
    Wave odd = waveOf(SignalType::Internal, DisplayFormat::Enumeration, 2, { { LogicVector(2), 0 } });
    odd.display.literals = { "'a'", "' '", "'%'" };     // character literals, a blank one among them
    data.subgraphs["u1"].subgraphs["deep"].signals["odd"] = odd;

    const VcdTrace trace = readText(written(data, "cpu", 30));
    EXPECT_EQ(trace.rootName, "cpu");
    EXPECT_EQ(trace.endTime, 30u);
    EXPECT_TRUE(trace.skipped.empty());
    expectSameWaveform(data, trace.waveform, "cpu");
    EXPECT_EQ(formatValue(trace.waveform.subgraphs.at("u1").subgraphs.at("deep").signals.at("odd"), LogicVector(1)), "' '");
}

TEST(VcdRoundTrip, ASimulatedDesignReadsBackAsItWasSimulated)
{
    TestUtil::Simulation simulation({
        R"(
            library ieee;
            use ieee.std_logic_1164.all;

            entity blinker is
                port (
                    clk : in std_logic;
                    led : out std_logic
                );
            end entity blinker;

            architecture rtl of blinker is
                signal state : std_logic := '0';
            begin
                process (clk)
                begin
                    if rising_edge(clk) then
                        state <= not state;
                    end if;
                end process;
                led <= state;
            end architecture rtl;
        )",
        R"(
            library ieee;
            use ieee.std_logic_1164.all;
            use ieee.numeric_std.all;

            entity top is
            end entity top;

            architecture rtl of top is
                type phase_t is (idle, run, done);

                component blinker
                    port (
                        clk : in std_logic;
                        led : out std_logic
                    );
                end component;

                signal clk : std_logic := '0';
                signal led : std_logic;
                signal phase : phase_t := idle;
                signal ticks : integer := -3;
                signal rotating : std_logic_vector(0 to 3) := "0001";
                signal busy : boolean := false;
                signal count : unsigned(7 downto 0) := (others => '0');
            begin
                process
                begin
                    clk <= '0';
                    wait for 2 fs;
                    clk <= '1';
                    wait for 2 fs;
                end process;

                u1 : blinker port map (clk => clk, led => led);

                process (clk)
                begin
                    if rising_edge(clk) then
                        count <= count + 1;
                        ticks <= ticks + 1;
                        rotating <= rotating(1 to 3) & rotating(0);
                        busy <= not busy;
                        case phase is
                            when idle => phase <= run;
                            when run => phase <= done;
                            when done => phase <= idle;
                        end case;
                    end if;
                end process;
            end architecture rtl;
        )",
    });

    constexpr simTime_t end = 40;
    simulation.graph->tick();
    WaveformRecorder recorder(simulation.graph->takeSnapshot());
    for (simTime_t time = 1; time <= end; ++time)
    {
        simulation.graph->tick();
        recorder.record(simulation.graph->takeSnapshot(), time);
    }
    const WaveformData& simulated = recorder.waveform();
    ASSERT_TRUE(simulated.signals.count("phase"));
    ASSERT_TRUE(simulated.subgraphs.count("u1"));
    EXPECT_GT(simulated.signals.at("count").samples.size(), 3u) << "the design runs";

    const VcdTrace trace = readText(written(simulated, "top", end));
    EXPECT_EQ(trace.rootName, "top");
    EXPECT_EQ(trace.endTime, end);
    expectSameWaveform(simulated, trace.waveform, "top");

    const Wave& phase = trace.waveform.signals.at("phase");
    EXPECT_EQ(formatValue(phase, phase.valueAt(end)), formatValue(simulated.signals.at("phase"), simulated.signals.at("phase").valueAt(end)));
    EXPECT_EQ(formatValue(trace.waveform.signals.at("ticks"), trace.waveform.signals.at("ticks").valueAt(0)), "-3");
    EXPECT_TRUE(trace.waveform.signals.at("rotating").display.ascending);
}

// ---- Files of other tools -------------------------------------------------------------------------

TEST(VcdRead, AFileOfAnotherSimulator)
{
    const VcdTrace trace = readText(R"($date Fri Oct 30 2026 $end
$version Icarus Verilog $end
$timescale 1ns $end
$scope module tb $end
$var reg 1 ! CLK $end
$var reg 8 " data[7:0] $end
$var integer 32 # n $end
$scope task t $end
$var wire 4 $ nibble [0:3] $end
$upscope $end
$upscope $end
$enddefinitions $end
$comment written by hand $end
#0
$dumpvars
X!
bx "
b11 #
bz1 $
$end
#2
1!
b1 "
#3
0!
b101 #
b1Z $
)");

    EXPECT_EQ(trace.rootName, "tb");
    EXPECT_EQ(trace.endTime, 3'000'000u) << "nanoseconds are a million femtoseconds";

    const Wave& clk = trace.waveform.signals.at("CLK");
    EXPECT_EQ(clk.display.format, DisplayFormat::Bit);
    EXPECT_EQ(clk.type, SignalType::Internal);
    EXPECT_EQ(clk.valueAt(0).str(1), "X");
    EXPECT_EQ(clk.valueAt(2'000'000).str(1), "1");
    EXPECT_EQ(clk.valueAt(3'000'000).str(1), "0");

    const Wave& data = trace.waveform.signals.at("data");
    EXPECT_EQ(data.width, 8);
    EXPECT_EQ(data.display.format, DisplayFormat::Hex);
    EXPECT_EQ(data.display.left, 7);
    EXPECT_FALSE(data.display.ascending);
    EXPECT_EQ(data.valueAt(0).str(8), "XXXXXXXX") << "an x on the left is repeated";
    EXPECT_EQ(data.valueAt(2'000'000).str(8), "00000001") << "a 0 or a 1 on the left is extended with 0";

    const Wave& n = trace.waveform.signals.at("n");
    EXPECT_EQ(n.display.format, DisplayFormat::SignedInteger);
    EXPECT_EQ(formatValue(n, n.valueAt(0)), "3");
    EXPECT_EQ(formatValue(n, n.valueAt(3'000'000)), "5");

    const Wave& nibble = trace.waveform.subgraphs.at("t").signals.at("nibble");
    EXPECT_EQ(nibble.display.left, 0);
    EXPECT_TRUE(nibble.display.ascending);
    EXPECT_EQ(nibble.valueAt(0).str(4), "ZZZ1") << "a z on the left is repeated";
    EXPECT_EQ(nibble.valueAt(3'000'000).str(4), "001Z");
}

TEST(VcdRead, NineValuedLogicFollowsOlogic)
{
    const VcdTrace trace = readText("$scope module top $end $var wire 7 ! v $end $upscope $end $enddefinitions $end\n"
                                    "#0 bulhw-x !\n");
    EXPECT_EQ(trace.waveform.signals.at("v").valueAt(0).str(7), "XX01XXX") << "the u on the left is repeated";
}

TEST(VcdRead, SeveralTopLevelScopesHangFromARoot)
{
    const VcdTrace trace = readText("$scope module a $end $var wire 1 ! x $end $upscope $end\n"
                                    "$scope module b $end $var wire 1 ! y $end $upscope $end\n"
                                    "$enddefinitions $end #0 1! #4 0!\n");
    EXPECT_EQ(trace.rootName, "root");
    ASSERT_EQ(trace.waveform.subgraphs.size(), 2u);
    EXPECT_EQ(trace.waveform.subgraphs.at("a").signals.at("x").valueAt(1).str(1), "1");
    EXPECT_EQ(trace.waveform.subgraphs.at("b").signals.at("y").valueAt(4).str(1), "0") << "one code, two variables";
}

TEST(VcdRead, ValuesAtOneTimeKeepTheLastAndUnsetVariablesAreUnknown)
{
    const VcdTrace trace = readText("$scope module top $end $var wire 1 ! a $end $var wire 1 \" b $end $var wire 1 # c $end "
                                    "$upscope $end $enddefinitions $end\n"
                                    "#0 0! 0#\n#5 1! 0! 1#\n#7 1!\n");
    const Wave& a = trace.waveform.signals.at("a");
    ASSERT_EQ(a.samples.size(), 2u) << "the glitch at 5 nets 0, and the 1 at 7 is a change";
    EXPECT_EQ(a.samples[1].timestamp, 7u);

    const Wave& b = trace.waveform.signals.at("b");
    ASSERT_EQ(b.samples.size(), 1u);
    EXPECT_EQ(b.valueAt(7).str(1), "X");

    const Wave& c = trace.waveform.signals.at("c");
    ASSERT_EQ(c.samples.size(), 2u);
    EXPECT_EQ(c.valueAt(5).str(1), "1");
}

TEST(VcdRead, AVariableFirstSetLaterIsUnknownUntilThen)
{
    const VcdTrace trace = readText("$scope module top $end $var wire 2 ! v $end $upscope $end $enddefinitions $end\n"
                                    "#6 b10 !\n");
    const Wave& v = trace.waveform.signals.at("v");
    EXPECT_EQ(v.valueAt(0).str(2), "XX");
    EXPECT_EQ(v.valueAt(6).str(2), "10");
}

TEST(VcdRead, VariablesPulseCannotShowAreSkipped)
{
    const VcdTrace trace = readText("$scope module top $end\n"
                                    "$var real 64 ! r $end\n"
                                    "$var wire 128 \" huge [127:0] $end\n"
                                    "$var wire 1 # ok $end\n"
                                    "$var wire 1 $ ok $end\n"
                                    "$upscope $end $enddefinitions $end\n"
                                    "#0 r1.5 ! b1 \" 1# 0$\n#1 r2 ! b10 \"\n");
    EXPECT_EQ(trace.waveform.signals.size(), 1u);
    EXPECT_EQ(trace.waveform.signals.at("ok").valueAt(1).str(1), "1");
    ASSERT_EQ(trace.skipped.size(), 3u);
    EXPECT_EQ(trace.skipped[0], "'top.r' is a real variable");
    EXPECT_EQ(trace.skipped[1], "'top.huge' is 128 bits wide (Pulse shows up to 64)");
    EXPECT_EQ(trace.skipped[2], "'top.ok' is declared twice; only its first declaration is shown");
}

TEST(VcdRead, AnEmptyDump)
{
    const VcdTrace trace = readText("$timescale 10 ps $end $enddefinitions $end\n");
    EXPECT_TRUE(trace.waveform.signals.empty());
    EXPECT_TRUE(trace.waveform.subgraphs.empty());
    EXPECT_EQ(trace.endTime, 0u);
}

// ---- Errors ---------------------------------------------------------------------------------------

TEST(VcdErrors, MalformedFilesNameTheirLine)
{
    const std::string header = "$scope module top $end\n$var wire 2 ! v $end\n$upscope $end\n$enddefinitions $end\n";

    EXPECT_EQ(readError(""), "1: the file ends before '$enddefinitions'");
    EXPECT_EQ(readError("$scope module top $end\n$var wire 1 ! x\n"), "2: '$var' has no '$end'");
    EXPECT_EQ(readError("$upscope $end"), "1: '$upscope' without an open '$scope'");
    EXPECT_EQ(readError("$timescale 3 days $end $enddefinitions $end"),
              "1: invalid '$timescale 3days': expected a number followed by s, ms, us, ns, ps or fs");
    EXPECT_EQ(readError("$var wire x ! v $end"), "1: invalid size 'x' of a variable");
    EXPECT_EQ(readError("$var wire 1 ! $end"), "1: expected '$var <type> <size> <identifier> <name> $end'");
    EXPECT_EQ(readError("clk $enddefinitions $end"), "1: unexpected 'clk' in the declarations of the file");

    EXPECT_EQ(readError(header + "#0\nb01 ?\n"), "6: unknown identifier code '?'");
    EXPECT_EQ(readError(header + "#5\nb01 !\n#4\n"), "7: timestamp '#4' goes back in time");
    EXPECT_EQ(readError(header + "#five\n"), "5: invalid timestamp '#five'");
    EXPECT_EQ(readError(header + "#99999999999999999999\n"), "5: invalid timestamp '#99999999999999999999'");
    EXPECT_EQ(readError("$timescale 1 s $end " + header + "#10000\n"),
              "5: timestamp '#10000' is too large (Pulse keeps time in femtoseconds, up to about 2.5 hours)");
    EXPECT_EQ(readError(header + "b101 !\n"), "5: the value 'b101' has 3 bits, but 'top.v' has 2");
    EXPECT_EQ(readError(header + "b2 !\n"), "5: invalid value 'b2': the bits of a value are 0, 1, x and z");
    EXPECT_EQ(readError(header + "b !\n"), "5: the value 'b' has no bits");
    EXPECT_EQ(readError(header + "b01\n"), "5: the vector value 'b01' has no identifier code");
    EXPECT_EQ(readError(header + "r1.0 !\n"), "5: the value 'r1.0' is not a logic value");
    EXPECT_EQ(readError(header + "$scope module x $end\n"), "5: unexpected '$scope' after '$enddefinitions'");
    EXPECT_EQ(readError(header + "1\n"), "5: unexpected '1'");
    EXPECT_EQ(readError(header + "$comment pulse-signal ! sideways bit $end\n"), "<no error>")
        << "after the declarations, comments are only comments";
    EXPECT_EQ(readError("$var wire 1 ! x $end $comment pulse-signal ! sideways bit $end"),
              "1: malformed '$comment pulse-signal'");
}

// ---- File names -----------------------------------------------------------------------------------

TEST(VcdPath, TheExtensionNamesTheFormat)
{
    EXPECT_TRUE(isVcdPath("out.vcd"));
    EXPECT_TRUE(isVcdPath("C:\\waves\\Run.VCD"));
    EXPECT_TRUE(isVcdPath("./a.b/dump.vcd"));
    EXPECT_FALSE(isVcdPath("dump.fst"));
    EXPECT_FALSE(isVcdPath("vcd"));
    EXPECT_FALSE(isVcdPath("dir.vcd/dump"));
}
