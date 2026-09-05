#ifndef PULSE_SHARED_TYPES_H
#define PULSE_SHARED_TYPES_H

#include <cstdint>

namespace Pulse
{
    typedef uint8_t bitWidth_t;                         /// Bit width type for signals and ports.
    static constexpr bitWidth_t BITWIDTH_DEFAULT = 64;  /// Default bit width for signals
    static constexpr bitWidth_t BITWIDTH_MAX = 64;      /// Maximum bit width for signals and ports.

    typedef uint64_t simTime_t; /// Simulation time type in femtoseconds (max before overflow: around 5 hours).
}

#endif // PULSE_SHARED_TYPES_H