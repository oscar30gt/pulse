#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <stdexcept>

#include "signals.h"
#include "logicVector.h"

using namespace Pulse;

// ============================================================================
// Helper Mock Classes for Testing Vector Signal Networks
// ============================================================================

/// Controllable output port representing an external driver, register, or source pin.
class MockSource : public ISignalEmitter
{
public:
    LogicVector state;

    explicit MockSource(LogicVector initial = LogicVector::HighZ(), bitWidth_t bitWidth = BITWIDTH_DEFAULT)
        : ISignalElement(bitWidth), ISignalEmitter(bitWidth), state(initial)
    {
    }

    [[nodiscard]] LogicVector read() const override
    {
        return state;
    }
};

/// Controllable input port that records notifications and resolution history.
class MockSink : public ISignalReceiver
{
public:
    int notifyCount = 0;
    LogicVector lastResolvedState = LogicVector::HighZ();
    ttl_t lastTtl = 0;

    explicit MockSink(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
        : ISignalElement(bitWidth), ISignalReceiver(bitWidth)
    {
    }

    bool notify(ttl_t ttl = TTL_DEFAULT) override
    {
        ++notifyCount;
        lastTtl = ttl;
        lastResolvedState = resolve();
        return (ttl == 0);
    }
};

/// Mock Inverter (bitwise NOT gate) connecting an input port to an output port.
class MockInverter : public ISignalReceiver, public ISignalEmitter
{
public:
    LogicVector outputState = LogicVector::HighZ();

    explicit MockInverter(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
        : ISignalElement(bitWidth), ISignalReceiver(bitWidth), ISignalEmitter(bitWidth)
    {
    }

    [[nodiscard]] LogicVector read() const override
    {
        return outputState;
    }

    bool notify(ttl_t ttl = TTL_DEFAULT) override
    {
        if (ttl == 0) return true;

        LogicVector in = resolve();
        outputState = ~in;

        bool ttlExpired = false;
        for (ISignalReceiver* target : m_targets)
        {
            if (target != nullptr)
                ttlExpired |= target->notify(ttl - 1);
        }
        return ttlExpired;
    }
};

/// Mock 2-Input bitwise AND Gate component.
class MockAndGate : public ISignalEmitter
{
public:
    class InputPin : public ISignalReceiver
    {
        MockAndGate* m_parent;
    public:
        InputPin(MockAndGate* parent, bitWidth_t bitWidth)
            : ISignalElement(bitWidth), ISignalReceiver(bitWidth), m_parent(parent)
        {
        }

        bool notify(ttl_t ttl = TTL_DEFAULT) override
        {
            return m_parent->onInputChanged(ttl);
        }
    };

    InputPin inA;
    InputPin inB;
    LogicVector outputState = LogicVector::HighZ();

    explicit MockAndGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
        : ISignalElement(bitWidth), ISignalEmitter(bitWidth), inA(this, bitWidth), inB(this, bitWidth)
    {
    }

    bool onInputChanged(ttl_t ttl)
    {
        if (ttl == 0) return true;

        LogicVector a = inA.resolve();
        LogicVector b = inB.resolve();
        outputState = a & b;

        bool ttlExpired = false;
        for (ISignalReceiver* target : m_targets)
        {
            if (target != nullptr)
                ttlExpired |= target->notify(ttl - 1);
        }
        return ttlExpired;
    }

    [[nodiscard]] LogicVector read() const override
    {
        return outputState;
    }
};

/// Mock 2-Input Adder component.
class MockAdder : public ISignalEmitter
{
public:
    class InputPin : public ISignalReceiver
    {
        MockAdder* m_parent;
    public:
        InputPin(MockAdder* parent, bitWidth_t bitWidth)
            : ISignalElement(bitWidth), ISignalReceiver(bitWidth), m_parent(parent)
        {
        }

        bool notify(ttl_t ttl = TTL_DEFAULT) override
        {
            return m_parent->onInputChanged(ttl);
        }
    };

    InputPin inA;
    InputPin inB;
    LogicVector outputState = LogicVector::HighZ();

    explicit MockAdder(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
        : ISignalElement(bitWidth), ISignalEmitter(bitWidth), inA(this, bitWidth), inB(this, bitWidth)
    {
    }

    bool onInputChanged(ttl_t ttl)
    {
        if (ttl == 0) return true;

        LogicVector a = inA.resolve();
        LogicVector b = inB.resolve();
        outputState = a + b;

        bool ttlExpired = false;
        for (ISignalReceiver* target : m_targets)
        {
            if (target != nullptr)
                ttlExpired |= target->notify(ttl - 1);
        }
        return ttlExpired;
    }

    [[nodiscard]] LogicVector read() const override
    {
        return outputState;
    }
};

// ============================================================================
// 1. Basic Port Connections and Bidirectional Wiring
// ============================================================================

TEST(SignalNetworkTest, ConnectInputAndOutput)
{
    MockSource source(LogicVector::Ones());
    MockSink sink;

    EXPECT_FALSE(sink.hasSource(&source));
    EXPECT_FALSE(source.hasTarget(&sink));

    sink.addSource(&source);

    EXPECT_TRUE(sink.hasSource(&source));
    EXPECT_TRUE(source.hasTarget(&sink));
}

TEST(SignalNetworkTest, AddTargetFromOutputPort)
{
    MockSource source(LogicVector::Zero());
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
// 2. Bit Width Validation and Type Safety
// ============================================================================

TEST(SignalNetworkTest, ElementWidthReporting)
{
    MockSource s64(LogicVector::Zero(), 64);
    MockSink k32(32);
    Signal w16(16);
    MockInverter inv8(8);

    EXPECT_EQ(s64.width(), 64);
    EXPECT_EQ(k32.width(), 32);
    EXPECT_EQ(w16.width(), 16);
    EXPECT_EQ(inv8.width(), 8);
}

TEST(SignalNetworkTest, MatchingBitWidthsConnectSuccessfully)
{
    MockSource src8(LogicVector::FromInt(0xAA), 8);
    Signal wire8(8);
    MockSink sink8(8);

    EXPECT_NO_THROW(wire8.addSource(&src8));
    EXPECT_NO_THROW(wire8.addTarget(&sink8));

    EXPECT_TRUE(wire8.hasSource(&src8));
    EXPECT_TRUE(wire8.hasTarget(&sink8));
}

TEST(SignalNetworkTest, MismatchedBitWidthAddSourceThrows)
{
    MockSource src16(LogicVector::Zero(), 16);
    MockSink sink32(32);

    EXPECT_THROW(sink32.addSource(&src16), std::invalid_argument);
    EXPECT_FALSE(sink32.hasSource(&src16));
    EXPECT_FALSE(src16.hasTarget(&sink32));
}

TEST(SignalNetworkTest, MismatchedBitWidthAddTargetThrows)
{
    MockSource src64(LogicVector::Zero(), 64);
    MockSink sink8(8);

    EXPECT_THROW(src64.addTarget(&sink8), std::invalid_argument);
    EXPECT_FALSE(src64.hasTarget(&sink8));
    EXPECT_FALSE(sink8.hasSource(&src64));
}

TEST(SignalNetworkTest, SignalWithMismatchedPortThrows)
{
    Signal bus32(32);
    MockSource src64(LogicVector::Zero(), 64);
    MockSink sink16(16);

    EXPECT_THROW(bus32.addSource(&src64), std::invalid_argument);
    EXPECT_THROW(bus32.addTarget(&sink16), std::invalid_argument);
}

// ============================================================================
// 3. Memory and Destruction Safety (Auto-Unlinking)
// ============================================================================

TEST(SignalNetworkTest, OutputPortDestructionUnlinksFromInputs)
{
    MockSink sink1;
    MockSink sink2;

    {
        MockSource scopedSource(LogicVector::Ones());
        scopedSource.addTarget(&sink1);
        scopedSource.addTarget(&sink2);

        EXPECT_TRUE(sink1.hasSource(&scopedSource));
        EXPECT_TRUE(sink2.hasSource(&scopedSource));
    } // scopedSource goes out of scope and is destroyed

    EXPECT_FALSE(sink1.hasSource(nullptr));
    EXPECT_EQ(sink1.resolve(), LogicVector::HighZ());
    EXPECT_EQ(sink2.resolve(), LogicVector::HighZ());
}

TEST(SignalNetworkTest, InputPortDestructionUnlinksFromOutputs)
{
    MockSource source1(LogicVector::Ones());
    MockSource source2(LogicVector::Zero());

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

TEST(SignalNetworkTest, SignalDestructionUnlinksBothSides)
{
    MockSource source(LogicVector::FromInt(0x1234));
    MockSink sink;

    {
        Signal scopedWire;
        scopedWire.addSource(&source);
        scopedWire.addTarget(&sink);

        EXPECT_TRUE(source.hasTarget(&scopedWire));
        EXPECT_TRUE(sink.hasSource(&scopedWire));
    }

    EXPECT_FALSE(source.hasTarget(nullptr));
    EXPECT_FALSE(sink.hasSource(nullptr));
    EXPECT_EQ(sink.resolve(), LogicVector::HighZ());
}

// ============================================================================
// 4. LogicVector Resolution Rules (Wired Bus Resolution)
// ============================================================================

TEST(SignalNetworkTest, ResolveEmptySourcesIsHighZ)
{
    MockSink sink;
    EXPECT_EQ(sink.resolve(), LogicVector::HighZ());
}

TEST(SignalNetworkTest, ResolveSingleSources)
{
    MockSink sink;
    MockSource source;
    sink.addSource(&source);

    source.state = LogicVector::Zero();
    EXPECT_EQ(sink.resolve(), LogicVector::Zero());

    source.state = LogicVector::Ones();
    EXPECT_EQ(sink.resolve(), LogicVector::Ones());

    source.state = LogicVector::HighZ();
    EXPECT_EQ(sink.resolve(), LogicVector::HighZ());

    source.state = LogicVector::Unknown();
    EXPECT_EQ(sink.resolve(), LogicVector::Unknown());

    source.state = LogicVector::FromInt(0xDEADBEEF01234567ULL);
    EXPECT_EQ(sink.resolve(), LogicVector::FromInt(0xDEADBEEF01234567ULL));
}

TEST(SignalNetworkTest, ResolveMultipleHighZSources)
{
    MockSink sink;
    MockSource s1(LogicVector::HighZ());
    MockSource s2(LogicVector::HighZ());
    MockSource s3(LogicVector::HighZ());

    sink.addSource(&s1);
    sink.addSource(&s2);
    sink.addSource(&s3);

    EXPECT_EQ(sink.resolve(), LogicVector::HighZ());
}

TEST(SignalNetworkTest, ResolveSingleActiveDriverWithHighZSources)
{
    MockSink sink;
    MockSource s1(LogicVector::HighZ());
    MockSource s2(LogicVector::FromInt(0xCAFEBABEULL));
    MockSource s3(LogicVector::HighZ());

    sink.addSource(&s1);
    sink.addSource(&s2);
    sink.addSource(&s3);

    EXPECT_EQ(sink.resolve(), LogicVector::FromInt(0xCAFEBABEULL));

    s2.state = LogicVector::Zero();
    EXPECT_EQ(sink.resolve(), LogicVector::Zero());
}

TEST(SignalNetworkTest, ResolveMultipleCompatibleDrivers)
{
    MockSink sink;
    MockSource s1(LogicVector::FromInt(0x12345678ULL));
    MockSource s2(LogicVector::FromInt(0x12345678ULL));

    sink.addSource(&s1);
    sink.addSource(&s2);

    EXPECT_EQ(sink.resolve(), LogicVector::FromInt(0x12345678ULL));

    s1.state = LogicVector::Zero();
    s2.state = LogicVector::Zero();
    EXPECT_EQ(sink.resolve(), LogicVector::Zero());
}

TEST(SignalNetworkTest, ResolveBusContentionToUnknownPerBit)
{
    MockSink sink;
    // s1 drives 0b1010 (10), s2 drives 0b1100 (12)
    MockSource s1(LogicVector::FromInt(0b1010ULL));
    MockSource s2(LogicVector::FromInt(0b1100ULL));

    sink.addSource(&s1);
    sink.addSource(&s2);

    LogicVector resolved = sink.resolve();
    // Bit 0: 0 vs 0 -> 0
    EXPECT_EQ(resolved.get(0), '0');
    // Bit 1: 1 vs 0 -> X (contention)
    EXPECT_EQ(resolved.get(1), 'X');
    // Bit 2: 0 vs 1 -> X (contention)
    EXPECT_EQ(resolved.get(2), 'X');
    // Bit 3: 1 vs 1 -> 1
    EXPECT_EQ(resolved.get(3), '1');
}

TEST(SignalNetworkTest, ResolveSplitBusMultiplexing)
{
    MockSink sink;
    // Driver A drives byte 0 (0x55), HighZ on byte 1 (v=1, m=1)
    MockSource driverA(LogicVector{ 0xFF55ULL, 0xFF00ULL });
    // Driver B HighZ on byte 0 (v=1, m=1), drives byte 1 (0xAA)
    MockSource driverB(LogicVector{ 0xAAFFULL, 0x00FFULL });

    sink.addSource(&driverA);
    sink.addSource(&driverB);

    LogicVector resolved = sink.resolve();
    EXPECT_EQ(resolved.value & 0xFFFFULL, 0xAA55ULL);
    EXPECT_EQ(resolved.mask & 0xFFFFULL, 0x0000ULL);
}

// ============================================================================
// 5. Signal Class and Wire Propagation
// ============================================================================

TEST(SignalNetworkTest, SignalInitialStateIsHighZ)
{
    Signal wire;
    EXPECT_EQ(wire.read(), LogicVector::HighZ());
}

TEST(SignalNetworkTest, SignalPropagatesFromSourceToSink)
{
    MockSource driver(LogicVector::FromInt(0xA5A5A5A5ULL));
    Signal wire;
    MockSink sink;

    wire.addSource(&driver);
    wire.addTarget(&sink);

    EXPECT_EQ(wire.read(), LogicVector::HighZ());
    EXPECT_EQ(sink.notifyCount, 0);

    bool expired = wire.notify();
    EXPECT_FALSE(expired);
    EXPECT_EQ(wire.read(), LogicVector::FromInt(0xA5A5A5A5ULL));
    EXPECT_EQ(sink.notifyCount, 1);
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(0xA5A5A5A5ULL));

    // Change driver to different value and notify
    driver.state = LogicVector::FromInt(0x5A5A5A5AULL);
    wire.notify();
    EXPECT_EQ(wire.read(), LogicVector::FromInt(0x5A5A5A5AULL));
    EXPECT_EQ(sink.notifyCount, 2);
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(0x5A5A5A5AULL));
}

TEST(SignalNetworkTest, SignalUnchangedStateDoesNotRePropagate)
{
    MockSource driver(LogicVector::FromInt(42));
    Signal wire;
    MockSink sink;

    wire.addSource(&driver);
    wire.addTarget(&sink);

    wire.notify();
    EXPECT_EQ(sink.notifyCount, 1);

    // Notify again without changing driver state
    bool expired = wire.notify();
    EXPECT_FALSE(expired);
    // Notify count should remain 1 because wire state didn't change
    EXPECT_EQ(sink.notifyCount, 1);
}

TEST(SignalNetworkTest, SignalChainPropagation)
{
    // Chain: Driver -> Wire1 -> Wire2 -> Wire3 -> Sink
    MockSource driver(LogicVector::FromInt(0x123456789ABCDEF0ULL));
    Signal wire1;
    Signal wire2;
    Signal wire3;
    MockSink sink;

    wire1.addSource(&driver);
    wire2.addSource(&wire1);
    wire3.addSource(&wire2);
    sink.addSource(&wire3);

    wire1.notify();

    EXPECT_EQ(wire1.read(), LogicVector::FromInt(0x123456789ABCDEF0ULL));
    EXPECT_EQ(wire2.read(), LogicVector::FromInt(0x123456789ABCDEF0ULL));
    EXPECT_EQ(wire3.read(), LogicVector::FromInt(0x123456789ABCDEF0ULL));
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(0x123456789ABCDEF0ULL));
}

TEST(SignalNetworkTest, SignalFanOut)
{
    // Fan-out: Driver -> Wire -> { SinkA, SinkB, SinkC }
    MockSource driver(LogicVector::FromInt(0xF0F0));
    Signal wire;
    MockSink sinkA;
    MockSink sinkB;
    MockSink sinkC;

    wire.addSource(&driver);
    wire.addTarget(&sinkA);
    wire.addTarget(&sinkB);
    wire.addTarget(&sinkC);

    wire.notify();

    EXPECT_EQ(wire.read(), LogicVector::FromInt(0xF0F0));
    EXPECT_EQ(sinkA.notifyCount, 1);
    EXPECT_EQ(sinkB.notifyCount, 1);
    EXPECT_EQ(sinkC.notifyCount, 1);
    EXPECT_EQ(sinkA.lastResolvedState, LogicVector::FromInt(0xF0F0));
    EXPECT_EQ(sinkB.lastResolvedState, LogicVector::FromInt(0xF0F0));
    EXPECT_EQ(sinkC.lastResolvedState, LogicVector::FromInt(0xF0F0));
}

TEST(SignalNetworkTest, TriStateBusMultiplexing)
{
    // Bus with 2 vector drivers enabled at different times
    MockSource driverA(LogicVector::FromInt(0xAAAA));
    MockSource driverB(LogicVector::HighZ());
    Signal bus;
    MockSink device;

    bus.addSource(&driverA);
    bus.addSource(&driverB);
    bus.addTarget(&device);

    // Driver A active, Driver B disabled (HighZ)
    bus.notify();
    EXPECT_EQ(bus.read(), LogicVector::FromInt(0xAAAA));
    EXPECT_EQ(device.lastResolvedState, LogicVector::FromInt(0xAAAA));

    // Switch active driver to Driver B (0x5555), Driver A disabled (HighZ)
    driverA.state = LogicVector::HighZ();
    driverB.state = LogicVector::FromInt(0x5555);
    bus.notify();
    EXPECT_EQ(bus.read(), LogicVector::FromInt(0x5555));
    EXPECT_EQ(device.lastResolvedState, LogicVector::FromInt(0x5555));

    // Both disabled (HighZ)
    driverB.state = LogicVector::HighZ();
    bus.notify();
    EXPECT_EQ(bus.read(), LogicVector::HighZ());
    EXPECT_EQ(device.lastResolvedState, LogicVector::HighZ());
}

// ============================================================================
// 6. Complex Gate and Vector Logic Networks
// ============================================================================

TEST(SignalNetworkTest, VectorInverterCircuit)
{
    // Driver -> WireIn -> Inverter -> WireOut -> Sink
    MockSource driver(LogicVector::FromInt(0x0F0F0F0F0F0F0F0FULL));
    Signal wireIn;
    MockInverter notGate;
    Signal wireOut;
    MockSink sink;

    wireIn.addSource(&driver);
    wireIn.addTarget(&notGate);
    wireOut.addSource(&notGate);
    wireOut.addTarget(&sink);

    wireIn.notify();
    EXPECT_EQ(wireIn.read(), LogicVector::FromInt(0x0F0F0F0F0F0F0F0FULL));
    EXPECT_EQ(notGate.read(), LogicVector::FromInt(~0x0F0F0F0F0F0F0F0FULL));
    EXPECT_EQ(wireOut.read(), LogicVector::FromInt(~0x0F0F0F0F0F0F0F0FULL));
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(~0x0F0F0F0F0F0F0F0FULL));

    // Change driver to all 1s
    driver.state = LogicVector::Ones();
    wireIn.notify();
    EXPECT_EQ(wireIn.read(), LogicVector::Ones());
    EXPECT_EQ(notGate.read(), LogicVector::Zero());
    EXPECT_EQ(wireOut.read(), LogicVector::Zero());
    EXPECT_EQ(sink.lastResolvedState, LogicVector::Zero());
}

TEST(SignalNetworkTest, VectorAndGateCircuit)
{
    MockSource driverA(LogicVector::FromInt(0xFF00FF00ULL));
    MockSource driverB(LogicVector::FromInt(0x0F0F0F0FULL));
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

    wireA.notify();
    wireB.notify();

    // 0xFF00FF00 & 0x0F0F0F0F = 0x0F000F00
    EXPECT_EQ(wireOut.read(), LogicVector::FromInt(0x0F000F00ULL));
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(0x0F000F00ULL));

    // Update driver A
    driverA.state = LogicVector::Ones();
    wireA.notify();
    EXPECT_EQ(wireOut.read(), LogicVector::FromInt(0x0F0F0F0FULL));
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(0x0F0F0F0FULL));
}

TEST(SignalNetworkTest, VectorAdderCircuit)
{
    MockSource driverA(LogicVector::FromInt(100));
    MockSource driverB(LogicVector::FromInt(250));
    Signal wireA;
    Signal wireB;
    MockAdder adder;
    Signal wireOut;
    MockSink sink;

    wireA.addSource(&driverA);
    wireA.addTarget(&adder.inA);

    wireB.addSource(&driverB);
    wireB.addTarget(&adder.inB);

    wireOut.addSource(&adder);
    wireOut.addTarget(&sink);

    wireA.notify();
    wireB.notify();

    EXPECT_EQ(wireOut.read(), LogicVector::FromInt(350));
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(350));

    // Update driver B with an unknown bit -> output becomes unknown
    driverB.state = LogicVector::Unknown();
    wireB.notify();
    EXPECT_EQ(wireOut.read(), LogicVector::Unknown());
    EXPECT_EQ(sink.lastResolvedState, LogicVector::Unknown());
}

TEST(SignalNetworkTest, MultiComponentPipelineCircuit)
{
    // Pipeline: (SrcA + SrcB) -> Adder -> Wire1 -> Inverter -> Wire2 -> Sink
    MockSource srcA(LogicVector::FromInt(10));
    MockSource srcB(LogicVector::FromInt(20));
    MockAdder adder;
    Signal wire1;
    MockInverter inverter;
    Signal wire2;
    MockSink sink;

    srcA.addTarget(&adder.inA);
    srcB.addTarget(&adder.inB);
    wire1.addSource(&adder);
    wire1.addTarget(&inverter);
    wire2.addSource(&inverter);
    wire2.addTarget(&sink);

    adder.inA.notify();
    adder.inB.notify();

    // 10 + 20 = 30 -> Inverted = ~30
    EXPECT_EQ(wire1.read(), LogicVector::FromInt(30));
    EXPECT_EQ(wire2.read(), LogicVector::FromInt(~30ULL));
    EXPECT_EQ(sink.lastResolvedState, LogicVector::FromInt(~30ULL));
}

// ============================================================================
// 7. TTL and Cycle / Oscillation Handling
// ============================================================================

TEST(SignalNetworkTest, NormalPropagationDoesNotExpireTTL)
{
    MockSource driver(LogicVector::Ones());
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
    // Create an inverting circular loop: InvA -> InvB -> InvA
    MockInverter invA;
    MockInverter invB;
    invA.outputState = LogicVector::Zero();

    invA.addTarget(&invB);
    invB.addTarget(&invA);

    // Notify with a small TTL (e.g. 5 steps). It must terminate without infinite recursion.
    bool expired = invA.notify(5);
    EXPECT_TRUE(expired);
}

TEST(SignalNetworkTest, RingOscillatorLoopTerminatesGracefully)
{
    // Inverter output connected back to its own input via a wire
    Signal feedbackWire;
    MockInverter inverter;
    inverter.outputState = LogicVector::Zero();

    feedbackWire.addSource(&inverter);
    feedbackWire.addTarget(&inverter);

    bool expired = feedbackWire.notify(8);
    EXPECT_TRUE(expired);
}
