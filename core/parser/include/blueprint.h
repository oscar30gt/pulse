#ifndef PULSE_BLUEPRINT_H
#define PULSE_BLUEPRINT_H

#include <string>
#include <unordered_map>
#include <memory>

#include "signalInterface.h"
#include "gates.h"
#include "shifter.h"
#include "comparator.h"

namespace Pulse::Parser
{
    // --------------------------------------------------------------------------------------------

    /// Definition of a component's port
    struct PortDefinition
    {
        bitWidth_t width;
        bool isOut;
    };

    /// Definition of a signal within a component
    struct SignalInstance
    {
        bitWidth_t width;
    };

    // --------------------------------------------------------------------------------------------

    class Blueprint;

    /// Abstract struct for component instances.
    /// A component instance defines, rather than its architecture, 
    /// the properties of a certain component instance.
    struct ComponentInstance
    {
        std::unordered_map<std::string, std::string> portMap;
        virtual ~ComponentInstance() = default;

    protected:
        // Protected constructor prevents manual instantiation
        ComponentInstance() = default;
    };

    /// Binary gate instance: NOT, AND, OR, XOR, NAND, NOR, XNOR
    struct BinaryGateInstance : ComponentInstance
    {
        Pulse::Engine::BinaryOp op;
    };

    /// Shifter instance: LSL, LSR, ASR, ROL, ROR
    struct ShifterInstance : ComponentInstance
    {
        Pulse::Engine::ShiftOp op;
    };

    /// Comparator instance: EQ, NE, LT, LE, GT, GE (signed or unsigned)
    struct ComparatorInstance : ComponentInstance
    {
        Pulse::Engine::CompareOp op;
        Pulse::Engine::CompareMode mode;
    };

    /// Splitter instance: extracts (high, low) range
    struct SplitterInstance : ComponentInstance
    {
        bitWidth_t high;
        bitWidth_t low;
    };

    /// Not gate instance
    struct NotGateInstance : ComponentInstance { };

    /// Merger (concatenator) instance
    struct MergerInstance : ComponentInstance { };

    /// Adder instance
    struct AdderInstance : ComponentInstance { };

    // Subtractor instance
    struct SubtractorInstance : ComponentInstance { };

    // Multiplicator instance
    struct MultiplicatorInstance : ComponentInstance { };

    /// Controlled buffer (tri-state buffer) instance
    struct ControlledBufferInstance : ComponentInstance { };

    /// Instance of a nested subgraph
    struct SubgraphInstance : ComponentInstance
    {
        Blueprint* bp;
    };

    // --------------------------------------------------------------------------------------------

    /// Defines the architecture of a component so it can
    /// be dynamically instantiated via the subgraph component
    struct Blueprint
    {
        std::unordered_map<std::string, PortDefinition> ports;
        std::unordered_map<std::string, SignalInstance> signals;
        std::unordered_map<std::string, std::unique_ptr<ComponentInstance>> components;

        /// Adds an input or output port definition to the blueprint.
        void addPort(std::string name, bitWidth_t width, bool isOut);

        /// Adds an internal wire/signal instance to the blueprint.
        void addSignal(std::string name, bitWidth_t width);

        /// Instantiates and registers a component of type T into the blueprint.
        /// Blueprint will take ownership of the added instance.
        void addComponent(std::string name, ComponentInstance* instance);
    };

    // --------------------------------------------------------------------------------------------

    void Blueprint::addPort(std::string name, bitWidth_t width, bool isOut)
    {
        ports[std::move(name)] = PortDefinition{ width, isOut };
    }

    void Blueprint::addSignal(std::string name, bitWidth_t width)
    {
        signals[std::move(name)] = SignalInstance{ width };
    }

    void Blueprint::addComponent(std::string name, ComponentInstance* instance)
    {
        if (instance)
        {
            components[std::move(name)] = std::unique_ptr<ComponentInstance>(instance);
        }
    }
}

#endif // PULSE_BLUEPRINT_H