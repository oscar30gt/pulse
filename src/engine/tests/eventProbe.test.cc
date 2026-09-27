// eventProbe.test.cc
// Tests for Pulse::Engine::EventProbe using GoogleTest.

#include <gtest/gtest.h>
#include "eventProbe.h"

using namespace Pulse;
using namespace Pulse::Engine;

TEST(EventProbeTest, OutputFollowsTheEventFlagOfTheInput)
{
    Wire in(4, LogicVector(0)), out(1);
    EventProbe probe(&in, &out);
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false));

    in.drive(LogicVector(3));
    probe.update();
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false)); // not an event before the wire updates

    in.update();
    probe.update();
    EXPECT_EQ(out.peek(), LogicVector::FromBool(true));

    in.update();
    probe.update();
    EXPECT_EQ(out.peek(), LogicVector::FromBool(false));
}

TEST(EventProbeTest, OutputMustBeOneBit)
{
    Wire in(1), out(4);
    EXPECT_THROW(EventProbe(&in, &out), bit_width_mismatch);
}
