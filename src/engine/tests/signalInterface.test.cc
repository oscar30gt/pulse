// signalInterface_test.cpp
// Exhaustive GoogleTest unit tests for the signal interface classes.

#include <gtest/gtest.h>
#include "signalInterface.h"
#include "logicVector.h"
#include "signalSource.h"
#include "signalDrain.h"

using namespace Pulse;
using namespace Pulse::Engine;

// Helper derived classes to expose protected members for testing.
class TestReceiver : public ISignalReceiver {
public:
    TestReceiver(bitWidth_t bitWidth = BITWIDTH_DEFAULT) : ISignalBase(bitWidth) {}
    using ISignalReceiver::ISignalReceiver;
    using ISignalReceiver::m_sources;
    // Simple resolve just returns default LogicVector.
    LogicVector resolve() const { return LogicVector(0); }
    bool onNotify(ttl_t ttl) override { return true; }
};

class TestEmitter : public ISignalEmitter {
public:
    TestEmitter(bitWidth_t bitWidth = BITWIDTH_DEFAULT) : ISignalBase(bitWidth) {}
    using ISignalEmitter::ISignalEmitter;
    using ISignalEmitter::m_targets;
    LogicVector peek() const override { return LogicVector(0); }
};

TEST(SignalInterfaceTest, BaseWidth) {
    ISignalBase base(32);
    EXPECT_EQ(base.width(), 32u);
    ISignalBase defaultBase;
    EXPECT_EQ(defaultBase.width(), BITWIDTH_DEFAULT);
}

TEST(SignalInterfaceTest, ReceiverSourceManagement) {
    TestReceiver recv(8);
    SignalSource src(8);
    // Initially no sources.
    EXPECT_FALSE(recv.hasSource(&src));
    // Add source via public API.
    recv.addSource(&src);
    EXPECT_TRUE(recv.hasSource(&src));
    // Adding same source again should have no effect.
    recv.addSource(&src);
    EXPECT_TRUE(recv.hasSource(&src));
    // Remove source.
    recv.removeSource(&src);
    EXPECT_FALSE(recv.hasSource(&src));
}

TEST(SignalInterfaceTest, EmitterTargetManagement) {
    TestEmitter emit(8);
    SignalDrain drain(8);
    EXPECT_FALSE(emit.hasTarget(&drain));
    emit.addTarget(&drain);
    EXPECT_TRUE(emit.hasTarget(&drain));
    // Adding same target again is safe.
    emit.addTarget(&drain);
    EXPECT_TRUE(emit.hasTarget(&drain));
    emit.removeTarget(&drain);
    EXPECT_FALSE(emit.hasTarget(&drain));
}

TEST(SignalInterfaceTest, CallbackInteraction) {
    // Ensure that adding/removing targets and sources updates both sides.
    SignalSource src(8);
    SignalDrain drain(8);
    // Connect source to drain via their interfaces.
    src.addTarget(&drain);
    EXPECT_TRUE(src.hasTarget(&drain));
    EXPECT_TRUE(drain.hasSource(&src));
    // Remove and verify.
    src.removeTarget(&drain);
    EXPECT_FALSE(src.hasTarget(&drain));
    EXPECT_FALSE(drain.hasSource(&src));
}
