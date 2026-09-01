#include <gtest/gtest.h>
#include "../include/component.h"
#include "../include/wire.h"

using namespace Pulse::Engine;

TEST(ComponentTest, PortExistence) {
    Component comp({{"in1", nullptr}, {"in2", nullptr}}, {{"out1", nullptr}});
    EXPECT_TRUE(comp.hasInputPort("in1"));
    EXPECT_TRUE(comp.hasInputPort("in2"));
    EXPECT_FALSE(comp.hasInputPort("in3"));
    EXPECT_TRUE(comp.hasOutputPort("out1"));
    EXPECT_FALSE(comp.hasOutputPort("out2"));
    EXPECT_TRUE(comp.hasPort("in1"));
    EXPECT_TRUE(comp.hasPort("out1"));
    EXPECT_FALSE(comp.hasPort("unknown"));
}
