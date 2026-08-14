#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include "signals.h"

using namespace Pulse::Engine;

// ============================================================================
// Helper Mock Classes for Testing Signal Networks
// ============================================================================

/// Controllable output port representing an external driver, clock, or source pin.
class MockSource : public ISignalEmitter
{
public:
    LogicState state;

    explicit MockSource(LogicState initial = LogicState::HighZ)
        : state(initial)
    {
    }

    [[nodiscard]] LogicState read() const override
    {
        return state;
    }
};

/// Controllable input port that records notifications and resolution history.
class MockSink : public ISignalReceiver
{
public:
    int notifyCount = 0;
    LogicState lastResolvedState = LogicState::HighZ;
    ttl_t lastTtl = 0;

    bool notify(ttl_t ttl = TTL_DEFAULT) override
    {
        ++notifyCount;
        lastTtl = ttl;
        lastResolvedState = resolve();
        return (ttl == 0);
    }
};

/// Mock Inverter (NOT Gate) connecting an input port to an output port.
class MockInverter : public ISignalReceiver, public ISignalEmitter
{
public:
    LogicState outputState = LogicState::HighZ;

    [[nodiscard]] LogicState read() const override
    {
        return outputState;
    }

    bool notify(ttl_t ttl = TTL_DEFAULT) override
    {
        if (ttl == 0) return true;

        LogicState in = resolve();
        if (in == LogicState::High)
            outputState = LogicState::Low;
        else if (in == LogicState::Low)
            outputState = LogicState::High;
        else
            outputState = in; // HighZ, Unknown, Uninitialized passed through

        bool ttlExpired = false;
        for (ISignalReceiver* target : m_targets)
        {
            if (target != nullptr)
                ttlExpired |= target->notify(ttl - 1);
        }
        return ttlExpired;
    }
};

/// Mock 2-Input AND Gate component.
class MockAndGate : public ISignalEmitter
{
public:
    class InputPin : public ISignalReceiver
    {
        MockAndGate* m_parent;
    public:
        explicit InputPin(MockAndGate* parent) : m_parent(parent) {}
        bool notify(ttl_t ttl = TTL_DEFAULT) override
        {
            return m_parent->onInputChanged(ttl);
        }
    };

    InputPin inA;
    InputPin inB;
    LogicState outputState = LogicState::HighZ;

    MockAndGate() : inA(this), inB(this) {}

    bool onInputChanged(ttl_t ttl)
    {
        if (ttl == 0) return true;

        LogicState a = inA.resolve();
        LogicState b = inB.resolve();

        if (a == LogicState::Low || b == LogicState::Low)
            outputState = LogicState::Low;
        else if (a == LogicState::High && b == LogicState::High)
            outputState = LogicState::High;
        else
            outputState = LogicState::Unknown;

        bool ttlExpired = false;
        for (ISignalReceiver* target : m_targets)
        {
            if (target != nullptr)
                ttlExpired |= target->notify(ttl - 1);
        }
        return ttlExpired;
    }

    [[nodiscard]] LogicState read() const override
    {
        return outputState;
    }
};

// ============================================================================
// 1. Basic Port Connections and Bidirectional Wiring
// ============================================================================

TEST(SignalNetworkTest, ConnectInputAndOutput)
{
    MockSource source(LogicState::High);
    MockSink sink;

    EXPECT_FALSE(sink.hasSource(&source));
    EXPECT_FALSE(source.hasTarget(&sink));

    sink.addSource(&source);

    EXPECT_TRUE(sink.hasSource(&source));
    EXPECT_TRUE(source.hasTarget(&sink));
}

TEST(SignalNetworkTest, AddTargetFromOutputPort)
{
    MockSource source(LogicState::Low);
    MockSink sink;

    source.addTarget(&sink);

    EXPECT_TRUE(source.hasTarget(&sink));
    EXPECT_TRUE(sink.hasSource(&source));
}

TEST(SignalNetworkTest, DisconnectViaInputPort)
{
    MockSource source;
    MockSink sink;

    sink.addSource(&source);
    EXPECT_TRUE(sink.hasSource(&source));

    sink.removeSource(&source);
    EXPECT_FALSE(sink.hasSource(&source));
    EXPECT_FALSE(source.hasTarget(&sink));
}

TEST(SignalNetworkTest, DisconnectViaOutputPort)
{
    MockSource source;
    MockSink sink;

    source.addTarget(&sink);
    EXPECT_TRUE(source.hasTarget(&sink));

    source.removeTarget(&sink);
    EXPECT_FALSE(source.hasTarget(&sink));
    EXPECT_FALSE(sink.hasSource(&source));
}

TEST(SignalNetworkTest, DuplicateConnectionsIgnored)
{
    MockSource source;
    MockSink sink;

    sink.addSource(&source);
    sink.addSource(&source); // Duplicate
    EXPECT_TRUE(sink.hasSource(&source));
    EXPECT_TRUE(source.hasTarget(&sink));

    // A single remove should completely disconnect
    sink.removeSource(&source);
    EXPECT_FALSE(sink.hasSource(&source));
    EXPECT_FALSE(source.hasTarget(&sink));
}

TEST(SignalNetworkTest, RemoveNonExistentConnectionIsSafe)
{
    MockSource source;
    MockSink sink;

    EXPECT_NO_THROW(sink.removeSource(&source));
    EXPECT_NO_THROW(source.removeTarget(&sink));
    EXPECT_NO_THROW(sink.removeSource(nullptr));
    EXPECT_NO_THROW(source.removeTarget(nullptr));
}

// ============================================================================
// 2. Memory and Destruction Safety (Auto-Unlinking)
// ============================================================================

TEST(SignalNetworkTest, OutputPortDestructionUnlinksFromInputs)
{
    MockSink sink1;
    MockSink sink2;

    {
        MockSource scopedSource(LogicState::High);
        scopedSource.addTarget(&sink1);
        scopedSource.addTarget(&sink2);

        EXPECT_TRUE(sink1.hasSource(&scopedSource));
        EXPECT_TRUE(sink2.hasSource(&scopedSource));
    } // scopedSource goes out of scope and is destroyed

    EXPECT_FALSE(sink1.hasSource(nullptr));
    EXPECT_EQ(sink1.resolve(), LogicState::HighZ);
    EXPECT_EQ(sink2.resolve(), LogicState::HighZ);
}

TEST(SignalNetworkTest, InputPortDestructionUnlinksFromOutputs)
{
    MockSource source1(LogicState::High);
    MockSource source2(LogicState::Low);

    {
        MockSink scopedSink;
        scopedSink.addSource(&source1);
        scopedSink.addSource(&source2);

        EXPECT_TRUE(source1.hasTarget(&scopedSink));
        EXPECT_TRUE(source2.hasTarget(&scopedSink));
    } // scopedSink goes out of scope and is destroyed

    EXPECT_FALSE(source1.hasTarget(nullptr));
    EXPECT_FALSE(source2.hasTarget(nullptr));
}

// ============================================================================
// 3. LogicState Resolution Rules (Wired Bus Resolution)
// ============================================================================

TEST(SignalNetworkTest, ResolveEmptySourcesIsHighZ)
{
    MockSink sink;
    EXPECT_EQ(sink.resolve(), LogicState::HighZ);
}

TEST(SignalNetworkTest, ResolveSingleSources)
{
    MockSink sink;
    MockSource source;
    sink.addSource(&source);

    source.state = LogicState::Low;
    EXPECT_EQ(sink.resolve(), LogicState::Low);

    source.state = LogicState::High;
    EXPECT_EQ(sink.resolve(), LogicState::High);

    source.state = LogicState::HighZ;
    EXPECT_EQ(sink.resolve(), LogicState::HighZ);

    source.state = LogicState::Unknown;
    EXPECT_EQ(sink.resolve(), LogicState::Unknown);

    source.state = LogicState::Uninitialized;
    EXPECT_EQ(sink.resolve(), LogicState::Uninitialized);
}

TEST(SignalNetworkTest, ResolveMultipleHighZSources)
{
    MockSink sink;
    MockSource s1(LogicState::HighZ);
    MockSource s2(LogicState::HighZ);
    MockSource s3(LogicState::HighZ);

    sink.addSource(&s1);
    sink.addSource(&s2);
    sink.addSource(&s3);

    EXPECT_EQ(sink.resolve(), LogicState::HighZ);
}

TEST(SignalNetworkTest, ResolveSingleActiveDriverWithHighZSources)
{
    MockSink sink;
    MockSource s1(LogicState::HighZ);
    MockSource s2(LogicState::High);
    MockSource s3(LogicState::HighZ);

    sink.addSource(&s1);
    sink.addSource(&s2);
    sink.addSource(&s3);

    EXPECT_EQ(sink.resolve(), LogicState::High);

    s2.state = LogicState::Low;
    EXPECT_EQ(sink.resolve(), LogicState::Low);
}

TEST(SignalNetworkTest, ResolveMultipleCompatibleDrivers)
{
    MockSink sink;
    MockSource s1(LogicState::High);
    MockSource s2(LogicState::High);

    sink.addSource(&s1);
    sink.addSource(&s2);

    EXPECT_EQ(sink.resolve(), LogicState::High);

    s1.state = LogicState::Low;
    s2.state = LogicState::Low;
    EXPECT_EQ(sink.resolve(), LogicState::Low);
}

TEST(SignalNetworkTest, ResolveBusContentionToUnknown)
{
    MockSink sink;
    MockSource s1(LogicState::High);
    MockSource s2(LogicState::Low);

    sink.addSource(&s1);
    sink.addSource(&s2);

    // Contention: High vs Low -> Unknown
    EXPECT_EQ(sink.resolve(), LogicState::Unknown);

    // Contention: High vs Unknown -> Unknown
    s2.state = LogicState::Unknown;
    EXPECT_EQ(sink.resolve(), LogicState::Unknown);

    // Contention: Low vs Uninitialized -> Unknown
    s1.state = LogicState::Low;
    s2.state = LogicState::Uninitialized;
    EXPECT_EQ(sink.resolve(), LogicState::Unknown);
}

// ============================================================================
// 4. Signal Class and Wire Propagation
// ============================================================================

TEST(SignalNetworkTest, SignalInitialStateIsHighZ)
{
    Signal wire;
    EXPECT_EQ(wire.read(), LogicState::HighZ);
}

TEST(SignalNetworkTest, SignalPropagatesFromSourceToSink)
{
    MockSource driver(LogicState::High);
    Signal wire;
    MockSink sink;

    wire.addSource(&driver);
    wire.addTarget(&sink);

    EXPECT_EQ(wire.read(), LogicState::HighZ);
    EXPECT_EQ(sink.notifyCount, 0);

    bool expired = wire.notify();
    EXPECT_FALSE(expired);
    EXPECT_EQ(wire.read(), LogicState::High);
    EXPECT_EQ(sink.notifyCount, 1);
    EXPECT_EQ(sink.lastResolvedState, LogicState::High);

    // Change driver to Low and notify
    driver.state = LogicState::Low;
    wire.notify();
    EXPECT_EQ(wire.read(), LogicState::Low);
    EXPECT_EQ(sink.notifyCount, 2);
    EXPECT_EQ(sink.lastResolvedState, LogicState::Low);
}

TEST(SignalNetworkTest, SignalChainPropagation)
{
    // Chain: Driver -> Wire1 -> Wire2 -> Wire3 -> Sink
    MockSource driver(LogicState::High);
    Signal wire1;
    Signal wire2;
    Signal wire3;
    MockSink sink;

    wire1.addSource(&driver);
    wire2.addSource(&wire1);
    wire3.addSource(&wire2);
    sink.addSource(&wire3);

    wire1.notify();

    EXPECT_EQ(wire1.read(), LogicState::High);
    EXPECT_EQ(wire2.read(), LogicState::High);
    EXPECT_EQ(wire3.read(), LogicState::High);
    EXPECT_EQ(sink.lastResolvedState, LogicState::High);
}

TEST(SignalNetworkTest, SignalFanOut)
{
    // Fan-out: Driver -> Wire -> { SinkA, SinkB, SinkC }
    MockSource driver(LogicState::Low);
    Signal wire;
    MockSink sinkA;
    MockSink sinkB;
    MockSink sinkC;

    wire.addSource(&driver);
    wire.addTarget(&sinkA);
    wire.addTarget(&sinkB);
    wire.addTarget(&sinkC);

    wire.notify();

    EXPECT_EQ(wire.read(), LogicState::Low);
    EXPECT_EQ(sinkA.notifyCount, 1);
    EXPECT_EQ(sinkB.notifyCount, 1);
    EXPECT_EQ(sinkC.notifyCount, 1);
    EXPECT_EQ(sinkA.lastResolvedState, LogicState::Low);
    EXPECT_EQ(sinkB.lastResolvedState, LogicState::Low);
    EXPECT_EQ(sinkC.lastResolvedState, LogicState::Low);
}

TEST(SignalNetworkTest, TriStateBusMultiplexing)
{
    // Bus with 2 drivers enabled at different times
    MockSource driverA(LogicState::High);
    MockSource driverB(LogicState::HighZ);
    Signal bus;
    MockSink device;

    bus.addSource(&driverA);
    bus.addSource(&driverB);
    bus.addTarget(&device);

    // Driver A active, Driver B disabled (HighZ)
    bus.notify();
    EXPECT_EQ(bus.read(), LogicState::High);
    EXPECT_EQ(device.lastResolvedState, LogicState::High);

    // Switch active driver to Driver B (Low), Driver A disabled (HighZ)
    driverA.state = LogicState::HighZ;
    driverB.state = LogicState::Low;
    bus.notify();
    EXPECT_EQ(bus.read(), LogicState::Low);
    EXPECT_EQ(device.lastResolvedState, LogicState::Low);

    // Both disabled (HighZ)
    driverB.state = LogicState::HighZ;
    bus.notify();
    EXPECT_EQ(bus.read(), LogicState::HighZ);
    EXPECT_EQ(device.lastResolvedState, LogicState::HighZ);
}

// ============================================================================
// 5. Complex Gate and Logic Networks
// ============================================================================

TEST(SignalNetworkTest, InverterCircuit)
{
    // Driver -> WireIn -> Inverter -> WireOut -> Sink
    MockSource driver(LogicState::Low);
    Signal wireIn;
    MockInverter notGate;
    Signal wireOut;
    MockSink sink;

    wireIn.addSource(&driver);
    wireIn.addTarget(&notGate);
    wireOut.addSource(&notGate);
    wireOut.addTarget(&sink);

    // Notify input wire
    wireIn.notify();
    EXPECT_EQ(wireIn.read(), LogicState::Low);
    EXPECT_EQ(notGate.read(), LogicState::High);
    EXPECT_EQ(wireOut.read(), LogicState::High);
    EXPECT_EQ(sink.lastResolvedState, LogicState::High);

    // Change driver to High
    driver.state = LogicState::High;
    wireIn.notify();
    EXPECT_EQ(wireIn.read(), LogicState::High);
    EXPECT_EQ(notGate.read(), LogicState::Low);
    EXPECT_EQ(wireOut.read(), LogicState::Low);
    EXPECT_EQ(sink.lastResolvedState, LogicState::Low);
}

TEST(SignalNetworkTest, AndGateCircuit)
{
    MockSource driverA(LogicState::Low);
    MockSource driverB(LogicState::Low);
    Signal wireA;
    Signal wireB;
    MockAndGate andGate;
    Signal wireOut;
    MockSink sink;

    wireA.addSource(&driverA);
    wireA.addTarget(&andGate.inA);

    wireB.addSource(&driverB);
    wireB.addTarget(&andGate.inB);

    wireOut.addSource(&andGate);
    wireOut.addTarget(&sink);

    // 0 AND 0 = 0
    wireA.notify();
    wireB.notify();
    EXPECT_EQ(wireOut.read(), LogicState::Low);

    // 1 AND 0 = 0
    driverA.state = LogicState::High;
    wireA.notify();
    EXPECT_EQ(wireOut.read(), LogicState::Low);

    // 1 AND 1 = 1
    driverB.state = LogicState::High;
    wireB.notify();
    EXPECT_EQ(wireOut.read(), LogicState::High);
    EXPECT_EQ(sink.lastResolvedState, LogicState::High);

    // 0 AND 1 = 0
    driverA.state = LogicState::Low;
    wireA.notify();
    EXPECT_EQ(wireOut.read(), LogicState::Low);
}

// ============================================================================
// 6. TTL and Cycle / Oscillation Handling
// ============================================================================

TEST(SignalNetworkTest, NormalPropagationDoesNotExpireTTL)
{
    MockSource driver(LogicState::High);
    Signal wire1;
    Signal wire2;
    wire1.addSource(&driver);
    wire2.addSource(&wire1);

    bool expired = wire1.notify(10);
    EXPECT_FALSE(expired);
}

TEST(SignalNetworkTest, ZeroTtlStopsImmediatelyAndReportsExpiration)
{
    Signal wire;
    bool expired = wire.notify(0);
    EXPECT_TRUE(expired);
}

TEST(SignalNetworkTest, CyclicSignalLoopTerminatesGracefully)
{
    // Create a circular wire loop: WireA -> WireB -> WireA
    Signal wireA;
    Signal wireB;

    wireA.addTarget(&wireB);
    wireB.addTarget(&wireA);

    // Notify with a small TTL (e.g. 5 steps). It must terminate without infinite recursion.
    bool expired = wireA.notify(5);
    EXPECT_TRUE(expired);
}

TEST(SignalNetworkTest, RingOscillatorLoopTerminatesGracefully)
{
    // Inverter output connected back to its own input via a wire
    Signal feedbackWire;
    MockInverter inverter;

    feedbackWire.addSource(&inverter);
    feedbackWire.addTarget(&inverter);

    bool expired = feedbackWire.notify(8);
    EXPECT_TRUE(expired);
}
