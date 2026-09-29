// eventProbe.test.cc
// Tests for Pulse::Engine::EventProbe using GoogleTest.

#include <gtest/gtest.h>
#include "eventProbe.h"

using namespace Pulse;
using namespace Pulse::Engine;

namespace
{
    /// One tick of the probe: latch, then publish.
    void tick(EventProbe& probe)
    {
        probe.latch();
        probe.publish();
    }
}

TEST(EventProbeTest, TheFirstTickHasNoEvent)
{
    Wire in(4, LogicVector(0)), out(1);
    EventProbe probe(&in, &out);
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false));

    in.drive(LogicVector(3));   // a change before the first tick is initialization, not an event
    tick(probe);
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false));
}

TEST(EventProbeTest, AChangeBetweenTwoTicksIsAnEventOfTheSecond)
{
    Wire in(4, LogicVector(0)), out(1);
    EventProbe probe(&in, &out);
    tick(probe);

    in.drive(LogicVector(3));
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false)) << "the probe does not see the change before the next tick";

    tick(probe);
    EXPECT_EQ(out.peek(), LogicVector::FromBool(true));

    tick(probe);
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false)) << "no change during the previous tick";
}

TEST(EventProbeTest, AValueThatComesBackIsNoEvent)
{
    Wire in(1, LogicVector::FromBool(false)), out(1);
    EventProbe probe(&in, &out);
    tick(probe);

    in.drive(LogicVector::FromBool(true));
    in.drive(LogicVector::FromBool(false));
    tick(probe);
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false));
}

TEST(EventProbeTest, LatchingDoesNotDriveTheOutput)
{
    Wire in(1, LogicVector::FromBool(false)), out(1);
    EventProbe probe(&in, &out);
    tick(probe);

    in.drive(LogicVector::FromBool(true));
    probe.latch();
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false));
    probe.publish();
    EXPECT_EQ(out.peek(), LogicVector::FromBool(true));
}

TEST(EventProbeTest, OutputMustBeOneBit)
{
    Wire in(1), out(4);
    EXPECT_THROW(EventProbe(&in, &out), bit_width_mismatch);
}
