#ifndef PULSE_LOGICVECTOR_H
#define PULSE_LOGICVECTOR_H

#include <cstdint>
#include <stdexcept>

namespace Pulse
{
    // ============================================================================
    // LogicVector: IEEE 1164 four-state logic vector
    // ============================================================================

    /// A 64-bit logic vector supporting IEEE 1164 four-state logic ('0', '1', 'X', 'Z').
    /// Uses a compact two-field encoding: value and mask fields in uint64_t for efficiency.
    /// Encoding scheme:
    ///   - Value=0, Mask=0: Logic '0'
    ///   - Value=1, Mask=0: Logic '1'
    ///   - Value=0, Mask=1: Logic 'X' (unknown)
    ///   - Value=1, Mask=1: Logic 'Z' (high impedance)
    struct LogicVector
    {
        uint64_t value;
        uint64_t mask;

        // -------- Default constructor -----------------------------------------------------------

        /// Default constructs a LogicVector with all bits set to 'Z' (high impedance).
        constexpr LogicVector() : value(~0ULL), mask(~0ULL) { }

        /// Custom value constructor with optional mask (default 0 for definite bits).
        constexpr LogicVector(uint64_t v, uint64_t m = 0ULL) : value(v), mask(m) { }

        // -------- Factory methods ---------------------------------------------------------------

        /// Creates a LogicVector with all bits set to logic '0'.
        [[nodiscard]] static constexpr LogicVector Zero();

        /// Creates a LogicVector with all bits set to logic '1'.
        [[nodiscard]] static constexpr LogicVector Ones();

        /// Creates a LogicVector with all bits set to 'X' (unknown).
        [[nodiscard]] static constexpr LogicVector Unknown();

        /// Creates a LogicVector with all bits set to 'Z' (high impedance).
        [[nodiscard]] static constexpr LogicVector HighZ();

        /// Creates a LogicVector from an unsigned 64-bit integer (all bits definite).
        /// @param val The unsigned integer value.
        /// @returns A LogicVector with value bits set to val and all mask bits cleared.
        [[nodiscard]] static constexpr LogicVector FromInt(uint64_t val);

        /// Creates a LogicVector from a boolean value.
        /// @param val True creates logic '1', false creates logic '0'.
        /// @returns A LogicVector representing the boolean as a single definite bit.
        [[nodiscard]] static constexpr LogicVector FromBool(bool val);

        // -------- Conversion operators ----------------------------------------------------------

        /// Converts the LogicVector to a boolean.
        /// Returns true unless all bits (value and mask) are zero.
        explicit operator bool() const;

        /// Converts the LogicVector to an unsigned 64-bit integer.
        explicit operator uint64_t() const;

        /// Converts the LogicVector to an unsigned 32-bit integer.
        /// @note Overflowing bits are clamped.
        explicit operator uint32_t() const;

        /// Converts the LogicVector to an unsigned 16-bit integer.
        /// @note Overflowing bits are clamped.
        explicit operator uint16_t() const;

        /// Converts the LogicVector to an unsigned 8-bit integer.
        /// @note Overflowing bits are clamped.
        explicit operator uint8_t() const;

        // -------- Equality operators ------------------------------------------------------------

        /// Checks if two LogicVectors are identical.
        /// @param other The LogicVector to compare with.
        /// @returns True if value and mask fields are equal.
        [[nodiscard]] bool operator==(const LogicVector& other) const;

        /// Checks if two LogicVectors are different.
        /// @param other The LogicVector to compare with.
        /// @returns True if value or mask fields differ.
        [[nodiscard]] bool operator!=(const LogicVector& other) const;

        // -------- Bitwise operators (branchless) ------------------------------------------------

        /// Bitwise AND operation.
        /// Propagates unknown and high-impedance states according to IEEE 1164 rules.
        [[nodiscard]] LogicVector operator&(const LogicVector& other) const;

        /// Bitwise OR operation.
        /// Propagates unknown and high-impedance states according to IEEE 1164 rules.
        [[nodiscard]] LogicVector operator|(const LogicVector& other) const;

        /// Bitwise XOR operation.
        /// Propagates unknown and high-impedance states according to IEEE 1164 rules.
        [[nodiscard]] LogicVector operator^(const LogicVector& other) const;

        /// Bitwise NOT operation.
        /// Inverts definite bits ('0' <-> '1'), unknown becomes unknown, high-Z becomes unknown.
        [[nodiscard]] LogicVector operator~() const;

        /// Addition operation (commutative XOR for definite bits).
        /// Returns unknown if either operand contains undefined bits.
        [[nodiscard]] LogicVector operator+(const LogicVector& other) const;

        // -------- Shift operations---------------------------------------------------------------

        /// Logical shift left (LSL).
        /// @param shamt Shift amount in bits. Negative values perform LSR instead.
        /// @param width Optional bit width to constrain the operation (default 64).
        /// @returns A new LogicVector shifted left by shamt bits.
        [[nodiscard]] LogicVector lsl(uint8_t shamt, uint8_t width = 64) const;

        /// Logical shift right (LSR).
        /// @param shamt Shift amount in bits. Negative values perform LSL instead.
        /// @param width Optional bit width to constrain the operation (default 64).
        /// @returns A new LogicVector shifted right by shamt bits.
        [[nodiscard]] LogicVector lsr(uint8_t shamt, uint8_t width = 64) const;

        /// Arithmetic shift right (ASR).
        /// Extends the sign bit (MSB) when shifting right.
        /// @param shamt Shift amount in bits. Negative values perform LSL instead.
        /// @param width Optional bit width to constrain the operation (default 64).
        /// @returns A new LogicVector shifted right arithmetically.
        [[nodiscard]] LogicVector asr(uint8_t shamt, uint8_t width = 64) const;

        /// Rotate left (ROL).
        /// @param shamt Rotate amount in bits. Negative values perform ROR instead.
        /// @param width Optional bit width to constrain the rotation (default 64).
        /// @returns A new LogicVector rotated left by shamt bits.
        [[nodiscard]] LogicVector rol(uint8_t shamt, uint8_t width = 64) const;

        /// Rotate right (ROR).
        /// @param shamt Rotate amount in bits. Negative values perform ROL instead.
        /// @param width Optional bit width to constrain the rotation (default 64).
        /// @returns A new LogicVector rotated right by shamt bits.
        [[nodiscard]] LogicVector ror(uint8_t shamt, uint8_t width = 64) const;

        // -------- Bit resolution ----------------------------------------------------------------

        /// Resolves two LogicVectors according to IEEE 1164 wired logic rules.
        /// Useful for modeling bus contention and tri-state logic merging.
        /// Rules:
        ///   - Equal bits remain unchanged
        ///   - Unknown ('X') propagates
        ///   - High-impedance ('Z') yields to the other value
        ///   - Conflicting values (0 vs 1, both driven) produce unknown ('X')
        /// @param other The other LogicVector to resolve with.
        /// @returns A resolved LogicVector following IEEE 1164 conventions.
        [[nodiscard]] LogicVector resolve(const LogicVector& other) const;

        // -------- Single bit access -------------------------------------------------------------

        /// Retrieves the state of a single bit.
        /// @param index The bit index (0-63).
        /// @returns The bit state: '0', '1', 'X' (unknown), or 'Z' (high impedance).
        /// @throws std::out_of_range if index > 63.
        [[nodiscard]] char getBit(uint8_t index) const;

        /// Sets a single bit to a specific state.
        /// @param index The bit index (0-63).
        /// @param state The state to set ('0', '1', 'X', or 'Z'). Invalid values are ignored.
        /// @throws std::out_of_range if index > 63.
        void setBit(uint8_t index, char state);

        // -------- Bit range access --------------------------------------------------------------

        /// Extracts a range of bits as a new LogicVector.
        /// Supports both standard (start >= end) and reversed (start < end) ranges.
        /// The extracted bits are placed in the lower bits of the returned vector.
        /// 
        /// Examples:
        ///   - getRange(7, 0): extract bits [7:0] (standard, high to low)
        ///   - getRange(0, 7): extract bits [0:7] with reversal (low to high, reversed)
        /// 
        /// @param start First bit index (0-63).
        /// @param end Second bit index (0-63). Can be higher or lower than start.
        /// @returns A new LogicVector containing the extracted bit range.
        /// @throws std::out_of_range if either index > 63.
        [[nodiscard]] LogicVector getRange(uint8_t start, uint8_t end) const;

        /// Sets a range of bits from another LogicVector.
        /// Supports both standard (start >= end) and reversed (start < end) ranges.
        /// The lower bits of the value parameter are used to fill the range.
        /// 
        /// Examples:
        ///   - setRange(7, 0, val): set bits [7:0] from val (standard, high to low)
        ///   - setRange(0, 7, val): set bits [0:7] from val with reversal (low to high, reversed)
        /// 
        /// @param start First bit index (0-63).
        /// @param end Second bit index (0-63). Can be higher or lower than start.
        /// @param value LogicVector providing the new state. Only the bits needed are used.
        /// @throws std::out_of_range if either index > 63.
        void setRange(uint8_t start, uint8_t end, const LogicVector& value);

        // -------- Misc --------------------------------------------------------------------------

        /// Sanitizes the LogicVector to a certain width by clearing bits above the specified width.
        /// @param width The desired width of the LogicVector.
        LogicVector sanitize(uint8_t width) const;
    };
}

// ============================================================================
// Implementation section
// ============================================================================

namespace Pulse
{
    // -------- LogicVector implementation: Factory methods ---------------------------------------

    constexpr LogicVector LogicVector::Zero() { return { 0ULL, 0ULL }; }
    constexpr LogicVector LogicVector::Ones() { return { ~0ULL, 0ULL }; }
    constexpr LogicVector LogicVector::Unknown() { return { 0ULL, ~0ULL }; }
    constexpr LogicVector LogicVector::HighZ() { return { ~0ULL, ~0ULL }; }
    constexpr LogicVector LogicVector::FromInt(uint64_t val) { return { val, 0ULL }; }
    constexpr LogicVector LogicVector::FromBool(bool val) { return val ? LogicVector{ 1ULL, 0ULL } : LogicVector::Zero(); }

    // -------- LogicVector implementation: Conversion operators ----------------------------------

    inline LogicVector::operator bool() const
    {
        return (value | mask) != 0;
    }

    inline LogicVector::operator uint64_t() const
    {
        return value;
    }

    inline LogicVector::operator uint32_t() const
    {
        return static_cast<uint32_t>(value & 0xFFFFFFFFULL);
    }

    inline LogicVector::operator uint16_t() const
    {
        return static_cast<uint16_t>(value & 0xFFFFULL);
    }

    inline LogicVector::operator uint8_t() const
    {
        return static_cast<uint8_t>(value & 0xFFULL);
    }

    // -------- LogicVector implementation: Equality operators ------------------------------------

    inline bool LogicVector::operator==(const LogicVector& other) const
    {
        return (value == other.value) && (mask == other.mask);
    }

    inline bool LogicVector::operator!=(const LogicVector& other) const
    {
        return !(*this == other);
    }

    // -------- LogicVector implementation: Bitwise operators -------------------------------------

    inline LogicVector LogicVector::operator&(const LogicVector& other) const
    {
        // AND truth table for IEEE 1164:
        // 0 & 0 = 0,  0 & 1 = 0,  0 & X = 0,  0 & Z = 0
        // 1 & 0 = 0,  1 & 1 = 1,  1 & X = X,  1 & Z = X
        // X & 0 = 0,  X & 1 = X,  X & X = X,  X & Z = X
        // Z & 0 = 0,  Z & 1 = X,  Z & X = X,  Z & Z = X

        uint64_t new_value = this->value & other.value;

        uint64_t result_0 = (~value & ~mask) | (~other.value & ~other.mask);
        uint64_t result_1 = (value & ~mask) & (other.value & ~other.mask);
        uint64_t result_definite = result_0 | result_1;

        return { result_1, ~result_definite };
    }

    inline LogicVector LogicVector::operator|(const LogicVector& other) const
    {
        // OR truth table for IEEE 1164:
        // 0 | 0 = 0,  0 | 1 = 1,  0 | X = X,  0 | Z = X
        // 1 | 0 = 1,  1 | 1 = 1,  1 | X = 1,  1 | Z = 1
        // X | 0 = X,  X | 1 = 1,  X | X = X,  X | Z = X
        // Z | 0 = X,  Z | 1 = 1,  Z | X = X,  Z | Z = X

        uint64_t result_1 = (value & ~mask) | (other.value & ~other.mask);
        uint64_t result_0 = (~value & ~mask) & (~other.value & ~other.mask);
        uint64_t result_definite = result_1 | result_0;

        return { result_1, ~result_definite };
    }

    inline LogicVector LogicVector::operator^(const LogicVector& other) const
    {
        // XOR truth table for IEEE 1164:
        // 0 ^ 0 = 0,  0 ^ 1 = 1,  0 ^ X = X,  0 ^ Z = X
        // 1 ^ 0 = 1,  1 ^ 1 = 0,  1 ^ X = X,  1 ^ Z = X
        // X ^ 0 = X,  X ^ 1 = X,  X ^ X = X,  X ^ Z = X
        // Z ^ 0 = X,  Z ^ 1 = X,  Z ^ X = X,  Z ^ Z = X

        uint64_t both_definite = (~mask) & (~other.mask);
        uint64_t res_value = (value ^ other.value) & both_definite;
        return { res_value, ~both_definite };
    }

    inline LogicVector LogicVector::operator~() const
    {
        // NOT: flip when definite, unknown when undefined
        // 0 -> 1,  1 -> 0,  X -> X,  Z -> X
        return { ~value & ~mask, mask };
    }

    inline LogicVector LogicVector::operator+(const LogicVector& other) const
    {
        // Addition (XOR for definite bits, unknown if either is undefined)
        bool both_definite = (mask == 0) && (other.mask == 0);
        if (!both_definite)
        {
            return LogicVector::Unknown();
        }
        return { value ^ other.value, 0ULL };
    }

    // -------- LogicVector implementation: Shift operations --------------------------------------

    inline LogicVector LogicVector::lsl(uint8_t shamt, uint8_t width) const
    {
        if (shamt >= 64 || shamt >= width)
        {
            return { 0ULL, 0ULL }; // Shifted out all bits
        }

        uint64_t wmask = ~0ULL >> (64 - width);
        uint64_t new_value = (value << shamt) & wmask;
        uint64_t new_mask = (mask << shamt) & wmask;

        return { new_value, new_mask };
    }

    inline LogicVector LogicVector::lsr(uint8_t shamt, uint8_t width) const
    {
        if (shamt >= 64 || shamt >= width)
        {
            return { 0ULL, 0ULL }; // Shifted out all bits
        }

        uint64_t wmask = ~0ULL >> (64 - width);
        uint64_t new_value = (value >> shamt) & wmask;
        uint64_t new_mask = (mask >> shamt) & wmask;
        return { new_value, new_mask };
    }

    inline LogicVector LogicVector::asr(uint8_t shamt, uint8_t width) const
    {
        if (width == 0) return { 0ULL, 0ULL };

        uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
        uint64_t sign_bit = 1ULL << (width - 1);
        bool is_negative = (width <= 64) && ((value & sign_bit) != 0);

        // Clamp shift amount to width to avoid UB and unify out-of-bounds logic
        uint8_t effective_shamt = (shamt >= width || shamt >= 64) ? width : shamt;

        // Shift logic vector components
        uint64_t new_value = (value & wmask) >> effective_shamt;
        uint64_t new_mask = (mask & wmask) >> effective_shamt;

        // Apply sign extension bitmask if the original sign bit was set
        if (is_negative)
        {
            uint64_t sign_ext = wmask << (width - effective_shamt);
            new_value |= sign_ext;
        }

        return { new_value & wmask, new_mask & wmask };
    }

    inline LogicVector LogicVector::rol(uint8_t shamt, uint8_t width) const
    {
        if (width == 0) return { 0ULL, 0ULL };

        shamt %= width;
        if (shamt == 0) return *this;

        uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
        uint64_t new_value = ((value << shamt) | ((value & wmask) >> (width - shamt))) & wmask;
        uint64_t new_mask  = ((mask << shamt) | ((mask & wmask) >> (width - shamt))) & wmask;

        return { new_value, new_mask };
    }

    inline LogicVector LogicVector::ror(uint8_t shamt, uint8_t width) const
    {
        if (width == 0) return { 0ULL, 0ULL };

        shamt %= width;
        if (shamt == 0) return *this;

        uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
        uint64_t new_value = ((value & wmask) >> shamt) | ((value << (width - shamt)) & wmask);
        uint64_t new_mask  = ((mask & wmask) >> shamt) | ((mask << (width - shamt)) & wmask);

        return { new_value, new_mask };
    }

    // -------- LogicVector implementation: Bit resolution ----------------------------------------

    inline LogicVector LogicVector::resolve(const LogicVector& other) const
    {
        uint64_t res_value = this->value & other.value;

        // Identify bit states for each operand
        uint64_t aIsZ = this->value & this->mask;           // High-Z: value=1, mask=1
        uint64_t bIsZ = other.value & other.mask;           // High-Z: value=1, mask=1
        uint64_t aIsX = this->mask & ~this->value;          // Unknown: value=0, mask=1
        uint64_t bIsX = other.mask & ~other.value;          // Unknown: value=0, mask=1

        // Bus contention: conflicting values where neither is High-Z
        uint64_t conflict = (this->value ^ other.value) & ~aIsZ & ~bIsZ;

        // Result mask: set if either is X, both are Z, or there's contention
        uint64_t res_mask = aIsX | bIsX | (aIsZ & bIsZ) | conflict;

        return LogicVector{ res_value, res_mask };
    }

    // -------- LogicVector implementation: Single bit access -------------------------------------

    inline char LogicVector::getBit(uint8_t index) const
    {
        if (index > 63) throw std::out_of_range("Bit index out of range. Valid range is 0-63.");

        bool isDefinite = (mask & (1ULL << index)) == 0;
        bool isLow = (value & (1ULL << index)) == 0;

        if (isDefinite) return isLow ? '0' : '1';
        else return isLow ? 'X' : 'Z';
    }

    inline void LogicVector::setBit(uint8_t index, char state)
    {
        if (index > 63) throw std::out_of_range("Bit index out of range. Valid range is 0-63.");

        switch (state)
        {
            case '0':
                value &= ~(1ULL << index);
                mask  &= ~(1ULL << index);
                break;
            case '1':
                value |= (1ULL << index);
                mask  &= ~(1ULL << index);
                break;
            case 'X':
                value &= ~(1ULL << index);
                mask  |= (1ULL << index);
                break;
            case 'Z':
                value |= (1ULL << index);
                mask  |= (1ULL << index);
                break;
            default:
                // Invalid state, do nothing
                break;
        }
    }

    // -------- LogicVector implementation: Bit range access --------------------------------------

    inline LogicVector LogicVector::getRange(uint8_t start, uint8_t end) const
    {
        if (start > 63 || end > 63) throw std::out_of_range("Bit range out of range. Valid range is 0-63.");

        uint8_t real_high = (start > end) ? start : end;
        uint8_t real_low = (start > end) ? end : start;
        uint8_t width = real_high - real_low + 1;

        uint8_t shamtLeft = 64 - real_high - 1;
        uint8_t shamtRight = 64 - real_high + real_low - 1;
        uint64_t new_value = (value << shamtLeft) >> shamtRight;
        uint64_t new_mask  = (mask << shamtLeft) >> shamtRight;

        // If range is reversed (start < end), reverse the extracted bits
        if (start < end)
        {
            uint64_t rev_val = 0, rev_mask = 0;
            for (uint8_t i = 0; i < width; ++i)
            {
                if ((new_value >> i) & 1ULL) rev_val |= (1ULL << (width - 1 - i));
                if ((new_mask >> i) & 1ULL)  rev_mask |= (1ULL << (width - 1 - i));
            }
            new_value = rev_val;
            new_mask = rev_mask;
        }

        return { new_value, new_mask };
    }

    inline void LogicVector::setRange(uint8_t start, uint8_t end, const LogicVector& newState)
    {
        if (start > 63 || end > 63) throw std::out_of_range("Bit range out of range. Valid range is 0-63.");

        uint8_t real_high = (start > end) ? start : end;
        uint8_t real_low = (start > end) ? end : start;
        uint8_t width = real_high - real_low + 1;

        uint64_t input_val = newState.value;
        uint64_t input_mask = newState.mask;

        // If range is reversed (start < end), reverse the input bits before placing
        if (start < end)
        {
            uint64_t rev_val = 0, rev_mask = 0;
            for (uint8_t i = 0; i < width; ++i)
            {
                if ((input_val >> i) & 1ULL) rev_val |= (1ULL << (width - 1 - i));
                if ((input_mask >> i) & 1ULL)  rev_mask |= (1ULL << (width - 1 - i));
            }
            input_val = rev_val;
            input_mask = rev_mask;
        }

        uint64_t range_mask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
        uint64_t shifted_mask = range_mask << real_low;

        // Clear the bits in the current LogicVector for the specified range
        value &= ~shifted_mask;
        mask  &= ~shifted_mask;

        // Set the new state for the specified range
        value |= (input_val & range_mask) << real_low;
        mask  |= (input_mask & range_mask) << real_low;
    }

    // -------- LogicVector implementation: Misc --------------------------------------------------

    inline LogicVector LogicVector::sanitize(uint8_t width) const
    {
        if (width >= 64) return *this; // No change needed for full width
        if (width == 0) return { 0ULL, 0ULL }; // All bits cleared for zero width

        uint64_t wmask = (1ULL << width) - 1ULL;
        return { value & wmask, mask & wmask };
    }
}

#endif // PULSE_LOGICVECTOR_H