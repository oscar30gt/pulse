#include <gtest/gtest.h>
#include "clock.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(ClockTest, BasicFunctionality)
{
    Wire outWire(1);
    Clock c(&outWire, false, 1);
    EXPECT_FALSE(outWire.peek());
    c.update();
    EXPECT_TRUE(outWire.peek());
    c.update();
    EXPECT_FALSE(outWire.peek());
}

TEST(ClockTest, CustomDurations)
{
    Wire outWire(1);
    Clock c(&outWire, false, 5);

    for (int i = 0; i < 100; ++i)
    {
        for (int i = 0; i < 5; ++i)
        {
            EXPECT_FALSE(outWire.peek());
            c.update();
        }

        for (int i = 0; i < 5; ++i)
        {
            EXPECT_TRUE(outWire.peek());
            c.update();
        }
    }
}

TEST(ClockTest, AsymmetricalCustomDurations)
{
    Wire outWire(1);
    Clock c(&outWire, true, 5, 3);

    for (int i = 0; i < 100; ++i)
    {
        for (int i = 0; i < 5; ++i)
        {
            EXPECT_TRUE(outWire.peek());
            c.update();
        }

        for (int i = 0; i < 3; ++i)
        {
            EXPECT_FALSE(outWire.peek());
            c.update();
        }
    }
}