#include <gtest/gtest.h>

#include "waveform.h"

namespace Pulse::Waveform
{
TEST(WaveformFormatting, PadsHexadecimalToSignalWidth)
{
    EXPECT_EQ(formatBusValue(LogicVector::FromInt(0xA), 8), "0x0A");
    EXPECT_EQ(formatBusValue(LogicVector::FromInt(0x1F), 5), "0x1F");
    EXPECT_EQ(formatBusValue(LogicVector::FromInt(0xFF), 5), "0x1F");
    EXPECT_EQ(formatBusValue(LogicVector::FromInt(0x1234), 16), "0x1234");
}

TEST(WaveformFormatting, MarksUnknownAndHighImpedanceValuesAsErrors)
{
    EXPECT_EQ(formatBusValue(LogicVector{0, 0b0100}, 4), "error");
    EXPECT_EQ(formatBusValue(LogicVector{0b0010, 0b0010}, 4), "error");
}

TEST(WaveformRecorder, StoresOnlyValueChangesIncludingNestedGraphs)
{
    Engine::SubgraphSnapshot child;
    child.wires["nested"] = {1, LogicVector::FromBool(false)};
    Engine::SubgraphSnapshot initial;
    initial.inputs["input"] = {8, LogicVector::FromInt(1)};
    initial.subgraphs["child"] = child;
    WaveformRecorder recorder(initial);

    recorder.record(initial, 1);
    initial.inputs["input"].second = LogicVector::FromInt(2);
    initial.subgraphs["child"].wires["nested"].second = LogicVector::FromBool(true);
    recorder.record(initial, 2);

    EXPECT_EQ(recorder.waveform().signals.at("input").samples.size(), 2u);
    EXPECT_EQ(recorder.waveform().signals.at("input").samples.back().timestamp, 2u);
    EXPECT_EQ(recorder.waveform().subgraphs.at("child").signals.at("nested").samples.size(), 2u);
}
}
