#ifndef PULSE_SYMBOL_TABLE_H
#define PULSE_SYMBOL_TABLE_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Pulse::Engine
{
    /// How the value of a signal is shown to the user (in the waveform viewer).
    enum class DisplayFormat : uint8_t
    {
        Bit,                ///< A single logic bit (std_logic)
        Boolean,            ///< false / true
        Hex,                ///< A logic vector, shown in hexadecimal and expandable into its bits
        SignedInteger,      ///< Two's complement integer, shown in decimal
        UnsignedInteger,    ///< Non-negative integer, shown in decimal
        Enumeration,        ///< Position of an enumeration literal, shown by its name
    };

    /// What the source language says about one signal of a blueprint.
    struct SignalSymbol
    {
        DisplayFormat format = DisplayFormat::Hex;
        std::vector<std::string> literals;  ///< Enumeration literals, by position
        int64_t left = 0;                   ///< Index of the leftmost bit of a vector
        bool ascending = false;             ///< `to` (true) or `downto` (false) numbering of the bits of a vector

        /// VHDL index of the bit stored at position `bit` (0 = least significant) of a vector of `width` bits.
        int64_t indexOfBit(uint8_t bit, uint8_t width) const
        {
            // The rightmost element is the least significant bit.
            const int64_t right = ascending ? left + width - 1 : left - width + 1;
            return ascending ? right - bit : right + bit;
        }
    };

    /// The symbols of a blueprint: how to display each of its ports and signals, by name. It is paired with every subgraph
    /// built from the blueprint, so a waveform knows what the values it records mean.
    struct SymbolTable
    {
        std::unordered_map<std::string, SignalSymbol> signals;

        /// The symbol of a signal, or nullptr when the table does not describe it.
        const SignalSymbol* find(const std::string& name) const
        {
            auto found = signals.find(name);
            return found == signals.end() ? nullptr : &found->second;
        }
    };
}

#endif // PULSE_SYMBOL_TABLE_H
