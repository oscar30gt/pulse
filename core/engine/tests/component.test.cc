#include <gtest/gtest.h>
#include "../include/component.h"
#include "../include/wire.h"

using namespace Pulse;

TEST(ComponentTest, PortExistence) {
    Component comp({"in1", "in2"}, {"out1"});
    EXPECT_TRUE(comp.hasInputPort("in1"));
    EXPECT_TRUE(comp.hasInputPort("in2"));
    EXPECT_FALSE(comp.hasInputPort("in3"));
    EXPECT_TRUE(comp.hasOutputPort("out1"));
    EXPECT_FALSE(comp.hasOutputPort("out2"));
    EXPECT_TRUE(comp.hasPort("in1"));
    EXPECT_TRUE(comp.hasPort("out1"));
    EXPECT_FALSE(comp.hasPort("unknown"));
}

TEST(ComponentTest, ConnectDisconnect) {
    Component comp({"in"}, {"out"});
    Wire wire;
    // Valid connection
    comp.connect("in", wire);
    EXPECT_NE(comp.getSignal("in"), nullptr);
    // Disconnect
    comp.disconnect("in");
    EXPECT_EQ(comp.getSignal("in"), nullptr);
}

TEST(ComponentTest, InvalidConnect) {
    Component comp({"in"}, {});
    Wire wire;
    EXPECT_THROW(comp.connect("nonexistent", wire), std::invalid_argument);
    EXPECT_THROW(comp.disconnect("nonexistent"), std::invalid_argument);
    EXPECT_THROW(comp.getSignal("nonexistent"), std::invalid_argument);
}
