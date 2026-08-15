#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <concepts>
#include <functional>

#include "port.h"
#include "signals.h"
#include "logicVector.h"

using namespace Pulse;

// ============================================================================
// Helper Mock Components for Testing Port Interactions
// ============================================================================

namespace
{
    /// Simple mock receiver to test OutputPort driving without member callbacks.
    class MockReceiver : public ISignalReceiver
    {
    public:
        int notifyCount = 0;
        ttl_t lastTtl = 0;
        bool returnResult = true;

        explicit MockReceiver(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
            : ISignalBase(bitWidth), ISignalReceiver(bitWidth)
        {
        }

        bool notify(ttl_t ttl = TTL_DEFAULT) override
        {
            ++notifyCount;
            lastTtl = ttl;
            return returnResult;
        }
    };

    /// Simple mock emitter to test InputPort source connection and resolution.
    class MockEmitter : public ISignalEmitter
    {
    public:
        LogicVector state;

        explicit MockEmitter(LogicVector initial = LogicVector::HighZ(), bitWidth_t bitWidth = BITWIDTH_DEFAULT)
            : ISignalBase(bitWidth), ISignalEmitter(bitWidth), state(initial)
        {
        }

        [[nodiscard]] LogicVector peek() const override
        {
            return state;
        }
    };

    /// Mock device component with member function callbacks bound to InputPorts.
    class MockDevice
    {
    public:
        int callbackCountA = 0;
        int callbackCountB = 0;
        ttl_t lastTtlA = 0;
        ttl_t lastTtlB = 0;
        LogicVector lastResolvedA = LogicVector::HighZ();
        LogicVector lastResolvedB = LogicVector::HighZ();

        InputPort inA;
        InputPort inB;
        OutputPort out;

        explicit MockDevice(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
            : inA(bitWidth, this, &MockDevice::onInputA),
              inB(bitWidth, this, &MockDevice::onInputB),
              out(bitWidth)
        {
        }

        void onInputA(ttl_t ttl)
        {
            ++callbackCountA;
            lastTtlA = ttl;
            lastResolvedA = inA.resolve();
        }

        void onInputB(ttl_t ttl)
        {
            ++callbackCountB;
            lastTtlB = ttl;
            lastResolvedB = inB.resolve();
        }
    };

    /// Mock reactive logic inverter that processes input and drives output upon notification.
    class MockReactiveInverter
    {
    public:
        InputPort input;
        OutputPort output;
        int executionCount = 0;

        explicit MockReactiveInverter(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
            : input(bitWidth, this, &MockReactiveInverter::onInputChanged),
              output(bitWidth)
        {
        }

        void onInputChanged(ttl_t ttl)
        {
            ++executionCount;
            if (ttl == 0) return;
            LogicVector val = input.resolve();
            output.drive(~val, ttl);
        }
    };

    /// Mock 2-input AND gate component.
    class MockReactiveAndGate
    {
    public:
        InputPort inA;
        InputPort inB;
        OutputPort out;
        int executionCount = 0;

        explicit MockReactiveAndGate(bitWidth_t bitWidth = BITWIDTH_DEFAULT)
            : inA(bitWidth, this, &MockReactiveAndGate::recalculate),
              inB(bitWidth, this, &MockReactiveAndGate::recalculate),
              out(bitWidth)
        {
        }

        void recalculate(ttl_t ttl)
        {
            ++executionCount;
            if (ttl == 0) return;
            LogicVector a = inA.resolve();
            LogicVector b = inB.resolve();
            out.drive(a & b, ttl);
        }
    };
}

// ============================================================================
// 1. PortCallback Concept Tests
// ============================================================================

namespace
{
    void freeFunctionCallback(ttl_t) {}
    int freeFunctionCallbackWithReturn(ttl_t) { return 0; }
    void freeFunctionNoArgs() {}
    void freeFunctionTwoArgs(ttl_t, int) {}
    void freeFunctionWrongType(std::string) {}

    struct FunctorCallback
    {
        void operator()(ttl_t) const {}
    };

    struct NonFunctor
    {
        int x;
    };
}

TEST(PortCallbackConceptTest, ValidCallablesSatisfyConcept)
{
    // Lambdas
    auto lambdaVal = [](ttl_t) {};
    auto lambdaInt = [](int) {};
    auto lambdaConstRef = [](const ttl_t&) {};
    auto lambdaWithReturn = [](ttl_t) -> bool { return true; };
    auto mutableLambda = [count = 0](ttl_t) mutable { ++count; };

    static_assert(PortCallback<decltype(lambdaVal)>);
    static_assert(PortCallback<decltype(lambdaInt)>);
    static_assert(PortCallback<decltype(lambdaConstRef)>);
    static_assert(PortCallback<decltype(lambdaWithReturn)>);
    static_assert(PortCallback<decltype(mutableLambda)>);

    // Free function pointers
    static_assert(PortCallback<decltype(&freeFunctionCallback)>);
    static_assert(PortCallback<decltype(&freeFunctionCallbackWithReturn)>);

    // Functors and std::function
    static_assert(PortCallback<FunctorCallback>);
    static_assert(PortCallback<std::function<void(ttl_t)>>);
    static_assert(PortCallback<std::function<int(ttl_t)>>);
}

TEST(PortCallbackConceptTest, InvalidTypesDoNotSatisfyConcept)
{
    // Non-callables
    static_assert(!PortCallback<int>);
    static_assert(!PortCallback<void*>);
    static_assert(!PortCallback<NonFunctor>);

    // Wrong number of arguments
    static_assert(!PortCallback<decltype(&freeFunctionNoArgs)>);
    static_assert(!PortCallback<decltype(&freeFunctionTwoArgs)>);

    // Incompatible parameter type
    static_assert(!PortCallback<decltype(&freeFunctionWrongType)>);
}

// ============================================================================
// 2. OutputPort Construction and Default Properties
// ============================================================================

TEST(OutputPortTest, DefaultConstruction)
{
    OutputPort port;
    EXPECT_EQ(port.width(), BITWIDTH_DEFAULT);
    EXPECT_EQ(port.width(), 64);
    EXPECT_EQ(port.peek(), LogicVector::HighZ());
}

TEST(OutputPortTest, CustomBitWidthConstruction)
{
    OutputPort port1(1);
    EXPECT_EQ(port1.width(), 1);
    EXPECT_EQ(port1.peek(), LogicVector::HighZ());

    OutputPort port8(8);
    EXPECT_EQ(port8.width(), 8);
    EXPECT_EQ(port8.peek(), LogicVector::HighZ());

    OutputPort port16(16);
    EXPECT_EQ(port16.width(), 16);
    EXPECT_EQ(port16.peek(), LogicVector::HighZ());

    OutputPort port32(32);
    EXPECT_EQ(port32.width(), 32);
    EXPECT_EQ(port32.peek(), LogicVector::HighZ());

    OutputPort port64(64);
    EXPECT_EQ(port64.width(), 64);
    EXPECT_EQ(port64.peek(), LogicVector::HighZ());
}

TEST(OutputPortTest, PolymorphismAsISignalEmitter)
{
    std::unique_ptr<ISignalEmitter> emitter = std::make_unique<OutputPort>(32);
    EXPECT_EQ(emitter->width(), 32);
    EXPECT_EQ(emitter->peek(), LogicVector::HighZ());
}

// ============================================================================
// 3. OutputPort State Management and Peek
// ============================================================================

TEST(OutputPortTest, PeekReturnsDrivenState)
{
    OutputPort port(64);

    port.drive(LogicVector::Zero());
    EXPECT_EQ(port.peek(), LogicVector::Zero());

    port.drive(LogicVector::Ones());
    EXPECT_EQ(port.peek(), LogicVector::Ones());

    port.drive(LogicVector::Unknown());
    EXPECT_EQ(port.peek(), LogicVector::Unknown());

    port.drive(LogicVector::HighZ());
    EXPECT_EQ(port.peek(), LogicVector::HighZ());

    LogicVector pattern = LogicVector::FromInt(0xDEADBEEF01234567ULL);
    port.drive(pattern);
    EXPECT_EQ(port.peek(), pattern);
}

TEST(OutputPortTest, SuccessiveDrivesOverwriteStateCorrectly)
{
    OutputPort port(8);

    port.drive(LogicVector::FromInt(0xAA));
    EXPECT_EQ(port.peek(), LogicVector::FromInt(0xAA));

    port.drive(LogicVector::FromInt(0x55));
    EXPECT_EQ(port.peek(), LogicVector::FromInt(0x55));

    port.drive(LogicVector::FromInt(0xFF));
    EXPECT_EQ(port.peek(), LogicVector::FromInt(0xFF));

    port.drive(LogicVector::FromInt(0x00));
    EXPECT_EQ(port.peek(), LogicVector::FromInt(0x00));
}

// ============================================================================
// 4. OutputPort Connections and Target Management
// ============================================================================

TEST(OutputPortTest, InitialTargetListIsEmpty)
{
    OutputPort port;
    EXPECT_FALSE(port.hasTarget(nullptr));
}

TEST(OutputPortTest, AddAndRemoveTarget)
{
    OutputPort port(16);
    MockReceiver target(16);

    port.addTarget(&target);
    EXPECT_TRUE(port.hasTarget(&target));
    EXPECT_TRUE(target.hasSource(&port));

    port.removeTarget(&target);
    EXPECT_FALSE(port.hasTarget(&target));
    EXPECT_FALSE(target.hasSource(&port));
}

TEST(OutputPortTest, DuplicateAddTargetIgnored)
{
    OutputPort port(8);
    MockReceiver target(8);

    port.addTarget(&target);
    port.addTarget(&target);
    EXPECT_TRUE(port.hasTarget(&target));

    port.removeTarget(&target);
    EXPECT_FALSE(port.hasTarget(&target));
    EXPECT_FALSE(target.hasSource(&port));
}

TEST(OutputPortTest, RemoveNonExistentTargetIsSafe)
{
    OutputPort port(32);
    MockReceiver target(32);

    EXPECT_NO_THROW(port.removeTarget(&target));
    EXPECT_NO_THROW(port.removeTarget(nullptr));
    EXPECT_FALSE(port.hasTarget(&target));
}

TEST(OutputPortTest, BitWidthMismatchThrowsOnAddTarget)
{
    OutputPort port16(16);
    MockReceiver target32(32);

    EXPECT_THROW(port16.addTarget(&target32), std::invalid_argument);
    EXPECT_FALSE(port16.hasTarget(&target32));
    EXPECT_FALSE(target32.hasSource(&port16));
}

TEST(OutputPortTest, DestructionUnlinksFromReceivers)
{
    MockReceiver r1(64);
    MockReceiver r2(64);

    {
        OutputPort scopedPort(64);
        scopedPort.addTarget(&r1);
        scopedPort.addTarget(&r2);

        EXPECT_TRUE(r1.hasSource(&scopedPort));
        EXPECT_TRUE(r2.hasSource(&scopedPort));
        EXPECT_TRUE(scopedPort.hasTarget(&r1));
        EXPECT_TRUE(scopedPort.hasTarget(&r2));
    } // scopedPort is destroyed here

    EXPECT_FALSE(r1.hasSource(nullptr));
    EXPECT_EQ(r1.resolve(), LogicVector::HighZ());
    EXPECT_EQ(r2.resolve(), LogicVector::HighZ());
}

// ============================================================================
// 5. OutputPort Drive & Signal Propagation
// ============================================================================

TEST(OutputPortTest, DriveWithNoTargetsReturnsFalse)
{
    OutputPort port(32);
    bool result = port.drive(LogicVector::FromInt(0x1234));
    EXPECT_FALSE(result);
    EXPECT_EQ(port.peek(), LogicVector::FromInt(0x1234));
}

TEST(OutputPortTest, DrivePropagatesToSingleTargetWithDecrementedTtl)
{
    OutputPort port(64);
    MockReceiver target(64);
    port.addTarget(&target);

    EXPECT_EQ(target.notifyCount, 0);

    bool result = port.drive(LogicVector::Ones(), 100);
    EXPECT_TRUE(result);
    EXPECT_EQ(target.notifyCount, 1);
    EXPECT_EQ(target.lastTtl, 99);
    EXPECT_EQ(port.peek(), LogicVector::Ones());
}

TEST(OutputPortTest, DriveUsesDefaultTtl)
{
    OutputPort port(64);
    MockReceiver target(64);
    port.addTarget(&target);

    port.drive(LogicVector::Zero()); // default TTL is TTL_DEFAULT (512)
    EXPECT_EQ(target.notifyCount, 1);
    EXPECT_EQ(target.lastTtl, TTL_DEFAULT - 1);
}

TEST(OutputPortTest, DrivePropagatesToMultipleTargets)
{
    OutputPort port(16);
    MockReceiver t1(16);
    MockReceiver t2(16);
    MockReceiver t3(16);

    port.addTarget(&t1);
    port.addTarget(&t2);
    port.addTarget(&t3);

    bool result = port.drive(LogicVector::FromInt(0xBEEF), 50);
    EXPECT_TRUE(result);

    EXPECT_EQ(t1.notifyCount, 1);
    EXPECT_EQ(t2.notifyCount, 1);
    EXPECT_EQ(t3.notifyCount, 1);

    EXPECT_EQ(t1.lastTtl, 49);
    EXPECT_EQ(t2.lastTtl, 49);
    EXPECT_EQ(t3.lastTtl, 49);
}

TEST(OutputPortTest, DriveAggregatesReturnValuesFromTargets)
{
    OutputPort port(8);
    MockReceiver rFalse1(8);
    MockReceiver rFalse2(8);
    rFalse1.returnResult = false;
    rFalse2.returnResult = false;

    port.addTarget(&rFalse1);
    port.addTarget(&rFalse2);

    // When all targets return false, drive returns false
    EXPECT_FALSE(port.drive(LogicVector::FromInt(0x01)));

    // When at least one target returns true, drive returns true
    MockReceiver rTrue(8);
    rTrue.returnResult = true;
    port.addTarget(&rTrue);

    EXPECT_TRUE(port.drive(LogicVector::FromInt(0x02)));
}

TEST(OutputPortTest, SuccessiveDrivesTriggerTargetMultipleTimes)
{
    OutputPort port(8);
    MockReceiver target(8);
    port.addTarget(&target);

    for (int i = 0; i < 10; ++i)
    {
        port.drive(LogicVector::FromInt(i));
        EXPECT_EQ(target.notifyCount, i + 1);
        EXPECT_EQ(port.peek(), LogicVector::FromInt(i));
    }
}

// ============================================================================
// 6. InputPort Construction and Default Properties
// ============================================================================

TEST(InputPortTest, DefaultConstruction)
{
    InputPort inPort;
    EXPECT_EQ(inPort.width(), BITWIDTH_DEFAULT);
    EXPECT_EQ(inPort.width(), 64);
    EXPECT_EQ(inPort.pull(), LogicVector::HighZ());
}

TEST(InputPortTest, CustomBitWidthConstruction)
{
    InputPort in1(1);
    EXPECT_EQ(in1.width(), 1);
    EXPECT_EQ(in1.pull(), LogicVector::HighZ());

    InputPort in8(8);
    EXPECT_EQ(in8.width(), 8);
    EXPECT_EQ(in8.pull(), LogicVector::HighZ());

    InputPort in16(16);
    EXPECT_EQ(in16.width(), 16);
    EXPECT_EQ(in16.pull(), LogicVector::HighZ());

    InputPort in32(32);
    EXPECT_EQ(in32.width(), 32);
    EXPECT_EQ(in32.pull(), LogicVector::HighZ());

    InputPort in64(64);
    EXPECT_EQ(in64.width(), 64);
    EXPECT_EQ(in64.pull(), LogicVector::HighZ());
}

TEST(InputPortTest, PolymorphismAsISignalReceiver)
{
    std::unique_ptr<ISignalReceiver> receiver = std::make_unique<InputPort>(32);
    EXPECT_EQ(receiver->width(), 32);
    EXPECT_EQ(receiver->resolve(), LogicVector::HighZ());
    EXPECT_FALSE(receiver->notify(10)); // Default constructed InputPort has no callback
}

// ============================================================================
// 7. InputPort Member Function Callback Binding & Invocation
// ============================================================================

TEST(InputPortTest, ConstructorWithValidMemberCallback)
{
    MockDevice device(32);
    EXPECT_EQ(device.inA.width(), 32);
    EXPECT_EQ(device.inB.width(), 32);
    EXPECT_EQ(device.callbackCountA, 0);
    EXPECT_EQ(device.callbackCountB, 0);

    bool resA = device.inA.notify(100);
    EXPECT_TRUE(resA);
    EXPECT_EQ(device.callbackCountA, 1);
    EXPECT_EQ(device.lastTtlA, 100);
    EXPECT_EQ(device.callbackCountB, 0);

    bool resB = device.inB.notify(200);
    EXPECT_TRUE(resB);
    EXPECT_EQ(device.callbackCountB, 1);
    EXPECT_EQ(device.lastTtlB, 200);
    EXPECT_EQ(device.callbackCountA, 1);
}

TEST(InputPortTest, NotifyWithoutCallbackReturnsFalse)
{
    InputPort port(64);
    bool result = port.notify(100);
    EXPECT_FALSE(result);
}

TEST(InputPortTest, ConstructorWithNullOwnerReturnsFalseOnNotify)
{
    MockDevice* nullDevice = nullptr;
    InputPort port(64, nullDevice, &MockDevice::onInputA);

    EXPECT_NO_THROW({
        bool result = port.notify(50);
        EXPECT_FALSE(result);
    });
}

TEST(InputPortTest, ConstructorWithNullMethodReturnsFalseOnNotify)
{
    MockDevice device(64);
    void (MockDevice::*nullMethod)(ttl_t) = nullptr;
    InputPort port(64, &device, nullMethod);

    EXPECT_NO_THROW({
        bool result = port.notify(50);
        EXPECT_FALSE(result);
    });
    EXPECT_EQ(device.callbackCountA, 0);
}

TEST(InputPortTest, SuccessiveNotificationsInvokeCallbackEachTime)
{
    MockDevice device(16);

    for (int i = 1; i <= 20; ++i)
    {
        ttl_t currentTtl = static_cast<ttl_t>(1000 - i);
        bool res = device.inA.notify(currentTtl);
        EXPECT_TRUE(res);
        EXPECT_EQ(device.callbackCountA, i);
        EXPECT_EQ(device.lastTtlA, currentTtl);
    }
}

// ============================================================================
// 8. InputPort Source Management and Resolution
// ============================================================================

TEST(InputPortTest, InitialSourceListIsEmpty)
{
    InputPort port;
    EXPECT_FALSE(port.hasSource(nullptr));
}

TEST(InputPortTest, AddAndRemoveSource)
{
    InputPort port(16);
    MockEmitter source(LogicVector::Ones(), 16);

    port.addSource(&source);
    EXPECT_TRUE(port.hasSource(&source));
    EXPECT_TRUE(source.hasTarget(&port));
    EXPECT_EQ(port.resolve(), LogicVector::Ones());

    port.removeSource(&source);
    EXPECT_FALSE(port.hasSource(&source));
    EXPECT_FALSE(source.hasTarget(&port));
    EXPECT_EQ(port.resolve(), LogicVector::HighZ());
}

TEST(InputPortTest, DuplicateAddSourceIgnored)
{
    InputPort port(8);
    MockEmitter source(LogicVector::Zero(), 8);

    port.addSource(&source);
    port.addSource(&source);
    EXPECT_TRUE(port.hasSource(&source));

    port.removeSource(&source);
    EXPECT_FALSE(port.hasSource(&source));
    EXPECT_FALSE(source.hasTarget(&port));
}

TEST(InputPortTest, RemoveNonExistentSourceIsSafe)
{
    InputPort port(32);
    MockEmitter source(LogicVector::Zero(), 32);

    EXPECT_NO_THROW(port.removeSource(&source));
    EXPECT_NO_THROW(port.removeSource(nullptr));
    EXPECT_FALSE(port.hasSource(&source));
}

TEST(InputPortTest, BitWidthMismatchThrowsOnAddSource)
{
    InputPort port8(8);
    MockEmitter source16(LogicVector::Zero(), 16);

    EXPECT_THROW(port8.addSource(&source16), std::invalid_argument);
    EXPECT_FALSE(port8.hasSource(&source16));
    EXPECT_FALSE(source16.hasTarget(&port8));
}

TEST(InputPortTest, DestructionUnlinksFromEmitters)
{
    MockEmitter e1(LogicVector::Ones(), 64);
    MockEmitter e2(LogicVector::Zero(), 64);

    {
        InputPort scopedPort(64);
        scopedPort.addSource(&e1);
        scopedPort.addSource(&e2);

        EXPECT_TRUE(e1.hasTarget(&scopedPort));
        EXPECT_TRUE(e2.hasTarget(&scopedPort));
        EXPECT_TRUE(scopedPort.hasSource(&e1));
        EXPECT_TRUE(scopedPort.hasSource(&e2));
    } // scopedPort destroyed here

    EXPECT_FALSE(e1.hasTarget(nullptr));
    EXPECT_FALSE(e2.hasTarget(nullptr));
}

TEST(InputPortTest, ResolveMultipleSources)
{
    InputPort inPort(8);
    MockEmitter e1(LogicVector::HighZ(), 8);
    MockEmitter e2(LogicVector::FromInt(0x55), 8);

    inPort.addSource(&e1);
    inPort.addSource(&e2);

    EXPECT_EQ(inPort.resolve(), LogicVector::FromInt(0x55));

    // Change e1 to drive compatible or conflicting state
    e1.state = LogicVector::FromInt(0xAA);
    // Resolving 0x55 and 0xAA results in conflicts (Unknown bits where 0 vs 1)
    EXPECT_EQ(inPort.resolve(), LogicVector::FromInt(0x55).resolve(LogicVector::FromInt(0xAA)));
}

TEST(InputPortTest, PullReturnsState)
{
    InputPort inPort(64);
    EXPECT_EQ(inPort.pull(), LogicVector::HighZ());
}

// ============================================================================
// 9. Integration: Direct OutputPort to InputPort Connections
// ============================================================================

TEST(PortIntegrationTest, DirectOutputToInputConnection)
{
    OutputPort outPort(64);
    MockDevice device(64);

    outPort.addTarget(&device.inA);
    EXPECT_TRUE(outPort.hasTarget(&device.inA));
    EXPECT_TRUE(device.inA.hasSource(&outPort));

    // Initial state before driving
    EXPECT_EQ(device.callbackCountA, 0);

    // Drive state onto OutputPort
    LogicVector drivenVal = LogicVector::FromInt(0xCAFEF00DULL);
    bool driveResult = outPort.drive(drivenVal, 100);

    EXPECT_TRUE(driveResult);
    EXPECT_EQ(device.callbackCountA, 1);
    EXPECT_EQ(device.lastTtlA, 99);
    EXPECT_EQ(device.lastResolvedA, drivenVal);
    EXPECT_EQ(device.inA.resolve(), drivenVal);
    EXPECT_EQ(outPort.peek(), drivenVal);
}

TEST(PortIntegrationTest, DirectOutputToMultipleInputPorts)
{
    OutputPort broadcaster(16);
    MockDevice device1(16);
    MockDevice device2(16);
    MockDevice device3(16);

    broadcaster.addTarget(&device1.inA);
    broadcaster.addTarget(&device2.inA);
    broadcaster.addTarget(&device3.inB);

    LogicVector val = LogicVector::FromInt(0x1234);
    broadcaster.drive(val, 20);

    EXPECT_EQ(device1.callbackCountA, 1);
    EXPECT_EQ(device2.callbackCountA, 1);
    EXPECT_EQ(device3.callbackCountB, 1);

    EXPECT_EQ(device1.lastResolvedA, val);
    EXPECT_EQ(device2.lastResolvedA, val);
    EXPECT_EQ(device3.lastResolvedB, val);

    EXPECT_EQ(device1.lastTtlA, 19);
    EXPECT_EQ(device2.lastTtlA, 19);
    EXPECT_EQ(device3.lastTtlB, 19);
}

TEST(PortIntegrationTest, OutputConnectedToInputViaSignalWire)
{
    OutputPort outPort(32);
    Signal wire(32);
    MockDevice device(32);

    outPort.addTarget(&wire);
    wire.addTarget(&device.inA);

    EXPECT_TRUE(outPort.hasTarget(&wire));
    EXPECT_TRUE(wire.hasSource(&outPort));
    EXPECT_TRUE(wire.hasTarget(&device.inA));
    EXPECT_TRUE(device.inA.hasSource(&wire));

    LogicVector state = LogicVector::FromInt(0x98765432ULL);
    outPort.drive(state, 50);

    EXPECT_EQ(wire.peek(), state);
    EXPECT_EQ(device.callbackCountA, 1);
    EXPECT_EQ(device.lastResolvedA, state);
    EXPECT_EQ(device.lastTtlA, 48); // outPort (50) -> wire (49) -> device.inA (48)
}

TEST(PortIntegrationTest, DisconnectingPortStopsNotifications)
{
    OutputPort outPort(8);
    MockDevice device(8);

    outPort.addTarget(&device.inA);

    outPort.drive(LogicVector::FromInt(1));
    EXPECT_EQ(device.callbackCountA, 1);

    outPort.removeTarget(&device.inA);

    outPort.drive(LogicVector::FromInt(2));
    EXPECT_EQ(device.callbackCountA, 1); // No new notification received
}

// ============================================================================
// 10. Integration: Reactive Circuits with InputPort & OutputPort
// ============================================================================

TEST(PortIntegrationTest, ReactiveInverterCircuit)
{
    // Chain: Driver (OutputPort) -> Inverter (InputPort -> OutputPort) -> Observer (MockDevice)
    OutputPort driver(8);
    MockReactiveInverter inverter(8);
    MockDevice observer(8);

    driver.addTarget(&inverter.input);
    inverter.output.addTarget(&observer.inA);

    // Drive 0x00 -> Inverter receives 0x00 -> Drives ~0x00 = 0xFF -> Observer receives 0xFF
    driver.drive(LogicVector::Zero(), 50);

    EXPECT_EQ(inverter.executionCount, 1);
    EXPECT_EQ(observer.callbackCountA, 1);
    EXPECT_EQ(observer.lastResolvedA, LogicVector::Ones());
    EXPECT_EQ(inverter.output.peek(), LogicVector::Ones());

    // Drive 0xAA -> Inverter drives 0x55
    driver.drive(LogicVector::FromInt(0xAA), 50);

    EXPECT_EQ(inverter.executionCount, 2);
    EXPECT_EQ(observer.callbackCountA, 2);
    EXPECT_EQ(observer.lastResolvedA, ~LogicVector::FromInt(0xAA));
}

TEST(PortIntegrationTest, ReactiveAndGateCircuit)
{
    OutputPort srcA(8);
    OutputPort srcB(8);
    MockReactiveAndGate andGate(8);
    MockDevice observer(8);

    srcA.addTarget(&andGate.inA);
    srcB.addTarget(&andGate.inB);
    andGate.out.addTarget(&observer.inA);

    // Step 1: srcA = 0xFF, srcB = HighZ (Initial)
    srcA.drive(LogicVector::Ones());
    EXPECT_EQ(andGate.executionCount, 1);

    // Step 2: srcB = 0x0F
    srcB.drive(LogicVector::FromInt(0x0F));
    EXPECT_EQ(andGate.executionCount, 2);
    EXPECT_EQ(observer.lastResolvedA, LogicVector::FromInt(0x0F));

    // Step 3: Change srcA to 0x33 -> 0x33 & 0x0F = 0x03
    srcA.drive(LogicVector::FromInt(0x33));
    EXPECT_EQ(andGate.executionCount, 3);
    EXPECT_EQ(observer.lastResolvedA, LogicVector::FromInt(0x03));
}

TEST(PortIntegrationTest, MultiStagePipelineCircuit)
{
    // Pipeline: Driver -> Inverter1 -> Inverter2 -> Inverter3 -> Receiver
    OutputPort driver(16);
    MockReactiveInverter inv1(16);
    MockReactiveInverter inv2(16);
    MockReactiveInverter inv3(16);
    MockDevice sink(16);

    driver.addTarget(&inv1.input);
    inv1.output.addTarget(&inv2.input);
    inv2.output.addTarget(&inv3.input);
    inv3.output.addTarget(&sink.inA);

    LogicVector testInput = LogicVector::FromInt(0x1234);
    driver.drive(testInput, 100);

    EXPECT_EQ(inv1.executionCount, 1);
    EXPECT_EQ(inv2.executionCount, 1);
    EXPECT_EQ(inv3.executionCount, 1);
    EXPECT_EQ(sink.callbackCountA, 1);

    // ~ (~ (~ 0x1234)) = ~0x1234
    EXPECT_EQ(sink.lastResolvedA, ~testInput);
    // TTL decrement: driver(100) -> inv1(99) -> inv2(98) -> inv3(97) -> sink(96)
    EXPECT_EQ(sink.lastTtlA, 96);
}

TEST(PortIntegrationTest, BusResolutionWithTriStateOutputPorts)
{
    // Two OutputPorts sharing a Signal bus connected to an InputPort
    OutputPort driverA(8);
    OutputPort driverB(8);
    Signal bus(8);
    MockDevice device(8);

    driverA.addTarget(&bus);
    driverB.addTarget(&bus);
    bus.addTarget(&device.inA);

    // Initial bus state
    EXPECT_EQ(device.inA.resolve(), LogicVector::HighZ());

    // Enable driver A with 0x42, driver B remains HighZ
    driverA.drive(LogicVector::FromInt(0x42));
    EXPECT_EQ(device.lastResolvedA, LogicVector::FromInt(0x42));

    // Switch: driver A goes HighZ, driver B drives 0x99
    driverA.drive(LogicVector::HighZ());
    driverB.drive(LogicVector::FromInt(0x99));
    EXPECT_EQ(device.lastResolvedA, LogicVector::FromInt(0x99));
}
