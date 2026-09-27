// waveform.test.cc — recording snapshots into waves, the display format each wave takes from the symbol table of the
// simulated design, the text of its values, and the waves of the bits of a logic vector.

#include <gtest/gtest.h>

#include "waveform.h"

using namespace Pulse;
using namespace Pulse::Engine;
using namespace Pulse::Debugger;

namespace
{
    Wave waveOf(DisplayFormat format, bitWidth_t width, std::vector<std::string> literals = {})
    {
        Wave wave;
        wave.width = width;
        wave.display.format = format;
        wave.display.literals = std::move(literals);
        return wave;
    }
}

// ---- Formats --------------------------------------------------------------------------------------

TEST(WaveformFormat, BitsAndBooleans)
{
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Bit, 1), LogicVector::FromBool(true)), "1");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Bit, 1), LogicVector::HighZ()), "Z");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Boolean, 1), LogicVector::FromBool(true)), "true");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Boolean, 1), LogicVector::FromBool(false)), "false");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Boolean, 1), LogicVector::Unknown()), "X");
}

TEST(WaveformFormat, LogicVectorsInHexadecimal)
{
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Hex, 8), LogicVector(0x3F)), "0x3F");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Hex, 12), LogicVector(0xA)), "0x00A");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Hex, 64), LogicVector(~0ULL)), "0xFFFFFFFFFFFFFFFF");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::Hex, 4), LogicVector(0b0101, 0b0100)), "error");
}

TEST(WaveformFormat, IntegersInDecimal)
{
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::SignedInteger, 32), LogicVector(0xFFFFFFF9)), "-7");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::SignedInteger, 32), LogicVector(12)), "12");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::UnsignedInteger, 8), LogicVector(0xFF)), "255");
    EXPECT_EQ(formatValue(waveOf(DisplayFormat::SignedInteger, 32), LogicVector(0, 1)), "X");
}

TEST(WaveformFormat, EnumerationsByLiteral)
{
    const Wave state = waveOf(DisplayFormat::Enumeration, 2, { "idle", "run", "done" });
    EXPECT_EQ(formatValue(state, LogicVector(1)), "run");
    EXPECT_EQ(formatValue(state, LogicVector(3)), "X") << "no literal at that position";
    EXPECT_EQ(formatWidth(state), 4u);
}

TEST(WaveformFormat, WidthsOfTheLongestText)
{
    EXPECT_EQ(formatWidth(waveOf(DisplayFormat::Bit, 1)), 1u);
    EXPECT_EQ(formatWidth(waveOf(DisplayFormat::Boolean, 1)), 5u);
    EXPECT_EQ(formatWidth(waveOf(DisplayFormat::SignedInteger, 32)), 11u);      // -2147483648
    EXPECT_EQ(formatWidth(waveOf(DisplayFormat::UnsignedInteger, 8)), 3u);
    EXPECT_EQ(formatWidth(waveOf(DisplayFormat::Hex, 16)), 6u);                 // 0xFFFF
}

// ---- Bits of vectors ------------------------------------------------------------------------------

TEST(WaveformBits, ABitWaveKeepsOnlyItsTransitions)
{
    Wave bus = waveOf(DisplayFormat::Hex, 4);
    bus.samples = { { LogicVector(0b0001), 0 }, { LogicVector(0b0011), 5 }, { LogicVector(0b0010), 9 }, { LogicVector(0b0110), 12 } };

    const Wave bit0 = bitWave(bus, 0);
    ASSERT_EQ(bit0.samples.size(), 2u);
    EXPECT_EQ(bit0.samples[1].timestamp, 9u);
    EXPECT_EQ(bit0.display.format, DisplayFormat::Bit);
    EXPECT_EQ(bit0.valueAt(10).str(1), "0");

    const Wave bit1 = bitWave(bus, 1);
    EXPECT_EQ(bit1.samples.size(), 2u);
    EXPECT_EQ(bit1.valueAt(6).str(1), "1");
}

TEST(WaveformBits, OnlyLogicVectorsExpand)
{
    EXPECT_TRUE(waveOf(DisplayFormat::Hex, 8).isExpandable());
    EXPECT_FALSE(waveOf(DisplayFormat::Hex, 1).isExpandable());
    EXPECT_FALSE(waveOf(DisplayFormat::SignedInteger, 32).isExpandable());
    EXPECT_TRUE(waveOf(DisplayFormat::Boolean, 1).isLevel());
    EXPECT_FALSE(waveOf(DisplayFormat::Enumeration, 1, { "a", "b" }).isLevel());
}

TEST(WaveformBits, TheIndexOfABitFollowsTheVectorsRange)
{
    SignalSymbol downto;
    downto.left = 7;
    downto.ascending = false;
    EXPECT_EQ(downto.indexOfBit(0, 8), 0);
    EXPECT_EQ(downto.indexOfBit(7, 8), 7);

    SignalSymbol to;
    to.left = 0;
    to.ascending = true;
    EXPECT_EQ(to.indexOfBit(0, 4), 3);     // the rightmost element is the least significant bit
    EXPECT_EQ(to.indexOfBit(3, 4), 0);
}

// ---- Recording ------------------------------------------------------------------------------------

TEST(WaveformRecorder, SignalsTakeTheirFormatFromTheSymbolTable)
{
    SymbolTable symbols;
    symbols.signals["n"] = SignalSymbol{ DisplayFormat::SignedInteger };

    SubgraphSnapshot snapshot;
    snapshot.symbols = &symbols;
    snapshot.wires["n"] = { 32, LogicVector(5) };
    snapshot.wires["b"] = { 1, LogicVector(1) };
    snapshot.wires["v"] = { 64, LogicVector(3) };

    WaveformRecorder recorder(snapshot);
    const WaveformData& data = recorder.waveform();
    EXPECT_EQ(data.signals.at("n").display.format, DisplayFormat::SignedInteger);
    EXPECT_EQ(data.signals.at("b").display.format, DisplayFormat::Bit);
    EXPECT_EQ(data.signals.at("v").display.format, DisplayFormat::Hex);
    EXPECT_EQ(data.signals.at("v").width, 64) << "a 64-bit wave keeps its width";
}

TEST(WaveformRecorder, OnlyTransitionsAreRecorded)
{
    SubgraphSnapshot snapshot;
    snapshot.wires["s"] = { 1, LogicVector(0) };
    WaveformRecorder recorder(snapshot);

    recorder.record(snapshot, 1);
    snapshot.wires["s"].second = LogicVector(1);
    recorder.record(snapshot, 2);
    recorder.record(snapshot, 3);

    const Wave& wave = recorder.waveform().signals.at("s");
    ASSERT_EQ(wave.samples.size(), 2u);
    EXPECT_EQ(wave.samples[1].timestamp, 2u);
}
