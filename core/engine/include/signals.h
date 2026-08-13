#include <cstdint>
#include <vector>
#include <functional>

namespace Pulse::Engine
{
    /// Logic state of a signal.
    enum class State : uint8_t
    {
        Low             = '0',  /// Logic 0 (0V)
        High            = '1',  /// Logic 1 (Vdd)
        HighZ           = 'Z',  /// High Impedance (floating)
        Unknown         = 'X',  /// Unknown (Error)
        Uninitialized   = 'U',  /// Uninitialized (Initial state)
    };

    // --------------------------------------------------------------------------------------------

    /// Input ports receive state from a signal.
    class InputPort
    {
        Signal* m_source = nullptr; /// Source signal connected to the input port.

    public:
        InputPort();
        virtual ~InputPort();

        /// Connects a signal to this port to receive state from.
        /// If already connected to a signal, it will be disconnected first.
        /// @param source Signal to connect to the port.
        void connectSource(Signal* source);

        /// Disconnects from the currently connected source signal, if any.
        void disconnectSource();

        /// Writes a new state to the port.
        /// @param newState New state for the port.
        /// @note This method should only be used if the component's internal
        /// logic does not depend on a signal. If a signal is already connected,
        /// it is recommended not to use this method as the port is already
        /// receiving state from the connected signal.
        virtual void write(State newState) = 0;
    };

    /// Output ports send state to a signal.
    class OutputPort
    {
        Signal* m_target = nullptr; /// Target signal connected to the output port.

    public:
        OutputPort();
        virtual ~OutputPort();

        /// Connects a signal to this port to send state to.
        /// If already connected to a signal, it will be disconnected first.
        /// @param target Signal to connect to the port.
        void connectTarget(Signal* target);

        /// Disconnects from the currently connected target signal, if any.
        void disconnectTarget();

        /// Reads the current state from the port.
        /// @returns State of the port.
        virtual State read() const = 0;
    };

    class Port final : public InputPort, public OutputPort
    {
        State m_state = State::HighZ;  /// Current state of the port.

    public:
        Port();
        ~Port() override;

        void write(State newState) override;
        State read() const override;
    };

    // --------------------------------------------------------------------------------------------

    /// Intermediate node or bus wire connecting components and ports.
    class Signal
    {
    private:
        State m_state = State::HighZ;           /// Current state of the signal.
        std::vector<SignalSource*> m_sources;   /// Source elements connected to the signal.
        std::vector<SignalTarget*> m_targets;   /// Target elements connected to the signal.

    public:
        Signal() = default;
        ~Signal() = default;

        State read() const;
        bool update(uint16_t);

        /// Connects a source to the signal. If the source is already connected,
        /// it will not be added again.
        /// @param source Source to connect to the signal.
        void connectSource(SignalSource* source);

        /// Disconnects a source from the signal. If the source is not connected,
        /// it will do nothing.
        /// @param source Source to disconnect from the signal.
        void disconnectSource(SignalSource* source);

        /// Connects a target to the signal. If the target is already connected,
        /// it will not be added again.
        /// @param target Target to connect to the signal.
        void connectTarget(SignalTarget* target);

        /// Disconnects a target from the signal. If the target is not connected,
        /// it will do nothing.
        /// @param target Target to disconnect from the signal.
        void disconnectTarget(SignalTarget* target);
    };
}