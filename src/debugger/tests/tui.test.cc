// tui.test.cc — the rows of the waveform viewer: components and logic vectors collapse and expand, and an expanded
// vector shows one row per bit, named after its VHDL index.

#include <gtest/gtest.h>

#include "tui.h"

using namespace Pulse;
using namespace Pulse::Engine;
using namespace Pulse::Debugger;

namespace
{
    WaveformData design()
    {
        WaveformData data;

        Wave bus;
        bus.width = 4;
        bus.display.format = DisplayFormat::Hex;
        bus.display.left = 3;
        bus.display.ascending = false;
        bus.samples = { { LogicVector(0b1010), 0 } };
        data.signals["count"] = bus;

        Wave flag;
        flag.width = 1;
        flag.display.format = DisplayFormat::Bit;
        flag.samples = { { LogicVector(1), 0 } };
        data.signals["flag"] = flag;

        data.subgraphs["u1"].signals["q"] = flag;
        return data;
    }

    std::vector<std::string> namesOf(const std::vector<Row>& rows)
    {
        std::vector<std::string> names;
        for (const Row& row : rows)
            names.push_back(row.name);
        return names;
    }
}

TEST(TuiRows, VectorsAndComponentsStartCollapsed)
{
    const WaveformData data = design();
    std::vector<Row> rows;
    std::map<std::string, Wave> bitWaves;
    collectRows(data, { "top" }, rows, "top", bitWaves);

    EXPECT_EQ(namesOf(rows), (std::vector<std::string>{ "▼ top", "▶ count", "flag", "▶ u1" }));
    EXPECT_TRUE(rows[1].isGraph) << "a vector can be expanded";
    EXPECT_NE(rows[1].wave, nullptr) << "and still shows its value";
    EXPECT_FALSE(rows[2].isGraph);
}

TEST(TuiRows, AnExpandedVectorShowsItsBitsLeftmostFirst)
{
    const WaveformData data = design();
    std::vector<Row> rows;
    std::map<std::string, Wave> bitWaves;
    collectRows(data, { "top", "top/count", "top/u1" }, rows, "top", bitWaves);

    EXPECT_EQ(namesOf(rows), (std::vector<std::string>{ "▼ top", "▼ count", "count(3)", "count(2)", "count(1)", "count(0)", "flag", "▼ u1", "q" }));
    ASSERT_NE(rows[2].wave, nullptr);
    EXPECT_EQ(rows[2].wave->valueAt(0).str(1), "1");
    EXPECT_EQ(rows[5].wave->valueAt(0).str(1), "0");
}
