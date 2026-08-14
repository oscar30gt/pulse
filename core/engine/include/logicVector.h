#ifndef PULSE_LOGICVECTOR_H
#define PULSE_LOGICVECTOR_H

#include <cstdint>

namespace Pulse
{
    /// Logic state of a signal.
    struct LogicVector
    {
        // Value    Mask     Meaning
        // 0        0        Logic 0
        // 1        0        Logic 1
        // 0        1        X (unknown)
        // 1        1        Z (high impedance)

        uint64_t value = ~0ULL; // Initialized as high impedance
        uint64_t mask  = ~0ULL;

        // -------- Factory methods ---------------------------------------------------------------

        static constexpr LogicVector Zero() { return { 0ULL, 0ULL }; }
        static constexpr LogicVector Ones() { return { ~0ULL, 0ULL }; }
        static constexpr LogicVector Unknown() { return { 0ULL, ~0ULL }; }
        static constexpr LogicVector HighZ() { return { ~0ULL, ~0ULL }; }
        static constexpr LogicVector FromInt(uint64_t val) { return { val, 0ULL }; }
        static constexpr LogicVector FromBool(bool val) { return val ? LogicVector{ 1ULL, 0ULL } : LogicVector::Zero(); }

        // -------- Operators ---------------------------------------------------------------------

        // (bool)LogicVector
        explicit operator bool() const
        {
            // A LogicVector is always considered true unless it is completely zero.
            return (value | mask) != 0;
        }

        [[nodiscard]]
        bool operator==(const LogicVector& other) const
        {
            return (value == other.value) && (mask == other.mask);
        }

        [[nodiscard]]
        bool operator!=(const LogicVector& other) const
        {
            return !(*this == other);
        }

        [[nodiscard]]
        LogicVector operator&(const LogicVector& other) const
        {
            //    0 1 X Z   |   Truth table for AND operation:
            //  0 0 0 0 0   |   0 dominates. Result is 1 only if both inputs are 1.
            //  1 0 1 X X   |   X and Z result in X when paired with 1 as it
            //  X 0 X X X   |   cannot be guaranteed to be 1.
            //  Z 0 X X X   |   

            uint64_t result_0 = (~value & ~mask) | (~other.value & ~other.mask);
            uint64_t result_1 = (value & ~mask) & (other.value & ~other.mask);
            uint64_t result_definite = result_0 | result_1;

            return { result_1, ~result_definite };
        }

        [[nodiscard]]
        LogicVector operator|(const LogicVector& other) const
        {
            // OR: 1 is dominant
            //    0 1 X Z   |   Truth table for OR operation:
            //  0 0 1 X X   |   1 dominates. Result is 0 only if both inputs are 0.
            //  1 1 1 1 1   |   X and Z result in X when paired with 0 as it
            //  X X 1 X X   |   cannot be guaranteed to be 0.
            //  Z X 1 X X   |   

            uint64_t result_1 = (value & ~mask) | (other.value & ~other.mask);
            uint64_t result_0 = (~value & ~mask) & (~other.value & ~other.mask);
            uint64_t result_definite = result_1 | result_0;

            return { result_1, ~result_definite };
        }

        [[nodiscard]]
        LogicVector operator^(const LogicVector& other) const
        {
            // XOR: both must be definite
            //    0 1 X Z   |   Truth table for XOR operation:
            //  0 0 1 X X   |   Definite values are inverted.
            //  1 1 0 X X   |   Other values result in X.
            //  X X X X X   |   
            //  Z X X X X   |  

            uint64_t both_definite = (~mask) & (~other.mask);
            uint64_t res_value = (value ^ other.value) & both_definite;
            return { res_value, ~both_definite };
        }

        [[nodiscard]]
        LogicVector operator~() const
        {
            // NOT: flip when definite, unknown when undefined
            //  0 -> 1
            //  1 -> 0
            //  X -> X
            //  Z -> X

            return { ~value & ~mask, mask };
        }

        [[nodiscard]]
        LogicVector operator+(const LogicVector& other) const
        {
            // Addition: only defined when both are definite
            //  0 + 0 = 0
            //  0 + 1 = 1
            //  1 + 0 = 1
            //  1 + 1 = 0
            //  X + ? = X
            //  Z + ? = X

            bool both_definite = mask == 0 && other.mask == 0;
            if (!both_definite)
            {
                return LogicVector::Unknown(); // Return unknown if either is not definite
            }

            return { value + other.value, 0ULL };
        }

        // -------- Method operators --------------------------------------------------------------

        /// Logical shift left (LSL) operation on the LogicVector.
        /// @param shamt Shift amount. Negative values will perform a logical shift right (LSR).
        /// @param width Optional width to work with. Default is 64 bits (full width).
        /// @returns A new LogicVector that is the result of the logical shift left operation.
        [[nodiscard]]
        LogicVector lsl(int8_t shamt, uint8_t width = 64) const
        {
            if (shamt < 0) return lsr(-shamt, width);
            if (width == 0) return { 0ULL, 0ULL };
            uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
            if (shamt >= width || shamt >= 64) return { 0ULL, 0ULL };
            uint64_t new_value = (value << shamt) & wmask;
            uint64_t new_mask = (mask << shamt) & wmask;
            return { new_value, new_mask };
        }

        /// Logical shift right (LSR) operation on the LogicVector.
        /// @param shamt Shift amount. Negative values will perform a logical shift left (LSL).
        /// @param width Optional width to work with. Default is 64 bits (full width).
        /// @returns A new LogicVector that is the result of the logical shift right operation.
        [[nodiscard]]
        LogicVector lsr(int8_t shamt, uint8_t width = 64) const
        {
            if (shamt < 0) return lsl(-shamt, width);
            if (width == 0) return { 0ULL, 0ULL };
            uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
            if (shamt >= width || shamt >= 64) return { 0ULL, 0ULL };
            uint64_t new_value = ((value & wmask) >> shamt) & wmask;
            uint64_t new_mask = ((mask & wmask) >> shamt) & wmask;
            return { new_value, new_mask };
        }

        /// Arithmetic shift right (ASR) operation on the LogicVector.
        /// @param shamt Shift amount. Negative values will perform a logical shift left (LSL).
        /// @param width Optional width to work with. Default is 64 bits (full width).
        /// @returns A new LogicVector that is the result of the arithmetic shift right operation.
        [[nodiscard]]
        LogicVector asr(int8_t shamt, uint8_t width = 64) const
        {
            if (shamt < 0) return lsl(-shamt, width);
            if (width == 0) return { 0ULL, 0ULL };
            uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
            bool is_negative = (width <= 64) && ((value & (1ULL << (width - 1))) != 0);

            if (shamt >= width || shamt >= 64)
            {
                return is_negative ? LogicVector{ wmask, 0ULL } : LogicVector{ 0ULL, 0ULL };
            }

            uint64_t new_value = ((value & wmask) >> shamt) & wmask;
            uint64_t new_mask = ((mask & wmask) >> shamt) & wmask;

            // If the original value was negative (sign bit set), extend the sign
            if (is_negative)
            {
                uint64_t sign_ext = (width - shamt >= 64) ? 0ULL : (~0ULL << (width - shamt));
                new_value = (new_value | sign_ext) & wmask;
            }

            return { new_value, new_mask };
        }

        /// Rotate left (ROL) operation on the LogicVector.
        /// @param shamt Shift amount. Negative values will perform a rotate right (ROR).
        /// @param width Optional width to work with. Default is 64 bits (full width).
        [[nodiscard]]
        LogicVector rol(int8_t shamt, uint8_t width = 64) const
        {
            if (shamt < 0) return ror(-shamt, width);
            if (width == 0) return { 0ULL, 0ULL };
            
            shamt %= width;
            if (shamt == 0) return *this;

            uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
            uint64_t new_value = ((value << shamt) | ((value & wmask) >> (width - shamt))) & wmask;
            uint64_t new_mask  = ((mask << shamt)  | ((mask & wmask)  >> (width - shamt))) & wmask;

            return { new_value, new_mask };
        }

        /// Rotate right (ROR) operation on the LogicVector.
        /// @param shamt Shift amount. Negative values will perform a rotate left (ROL).
        /// @param width Optional width to work with. Default is 64 bits (full width).
        [[nodiscard]]
        LogicVector ror(int8_t shamt, uint8_t width = 64) const
        {
            if (shamt < 0) return rol(-shamt, width);
            if (width == 0) return { 0ULL, 0ULL };
            
            shamt %= width;
            if (shamt == 0) return *this;

            uint64_t wmask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
            uint64_t new_value = ((value & wmask) >> shamt) | ((value << (width - shamt)) & wmask);
            uint64_t new_mask  = ((mask & wmask) >> shamt) | ((mask << (width - shamt)) & wmask);

            return { new_value, new_mask };
        }

        /// Merges this LogicVector with another, combining their values and masks.
        /// Merging two LogicVectors is useful when various outputs write to the same signal, which can
        /// lead to many errors. This method will safely combine the two LogicVectors following this rules:
        /// - If two bits are equal, the result will be that value.
        /// - If one bit is unknown (X), the result will be unknown (X).
        /// - If one bit is high impedance (Z), the result will be the other value.
        /// - If two bits are different (0 and 1), the result will be unknown (X).
        /// @param other The other LogicVector to merge with.
        /// @returns Resolved LogicVector.
        [[nodiscard]]
        LogicVector resolve(const LogicVector& other) const
        {
            // 1. Resulting value: bitwise AND between both values
            uint64_t res_value = this->value & other.value;

            // 2. State identification:
            // High-Z (Z) requires v=1 and m=1
            uint64_t aIsZ = this->value & this->mask;
            uint64_t bIsZ = other.value & other.mask;

            // Unknown (X) requires v=0 and m=1
            uint64_t aIsX = this->mask & ~this->value;
            uint64_t bIsX = other.mask & ~other.value;

            // 3. Bus contention / conflict (0 vs 1):
            // Conflict occurs when values differ and neither driven bit is High-Z
            uint64_t conflict = (this->value ^ other.value) & ~aIsZ & ~bIsZ;

            // 4. Resulting mask:
            // Mask bit is set if either side is X, both sides are Z (Z & Z = Z), or a contention occurs
            uint64_t res_mask = aIsX | bIsX | (aIsZ & bIsZ) | conflict;

            return LogicVector{ res_value, res_mask };
        }

        // -------- Getters -----------------------------------------------------------------------

        /// Returns the width of the LogicVector in bits.
        /// @param index The bit index to retrieve (0-63).
        /// @returns The width of the LogicVector in bits.
        [[nodiscard]]
        uint8_t get(uint8_t index) const
        {
            if (index >= 64) return 0; // Out of bounds, return 0
            bool isDefinite = (mask & (1ULL << index)) == 0;
            bool isLow = (value & (1ULL << index)) == 0;

            if (isDefinite) return isLow ? '0' : '1';
            else return isLow ? 'X' : 'Z';
        }

        /// Returns a range of bits from the LogicVector as a new LogicVector.
        /// @param start The starting bit index (0-63).
        /// @param end The ending bit index (0-63). Must be greater than or equal to start.
        /// @returns A new LogicVector representing the specified range of bits.
        [[nodiscard]]
        LogicVector getRange(uint8_t start, uint8_t end) const
        {
            if (start >= 64 || end >= 64 || start > end) return LogicVector::Unknown();

            uint8_t shamtLeft = 64 - end - 1, shamtRight = 64 - end + start - 1;
            uint64_t new_value = (value << shamtLeft) >> shamtRight;
            uint64_t new_mask  = (mask  << shamtLeft) >> shamtRight;

            return { new_value, new_mask };
        }

        // -------- Setters -----------------------------------------------------------------------

        /// Sets a specific bit in the LogicVector to a given state.
        /// @param index The bit index to set (0-63).
        /// @param state The state to set the bit to ('0', '1', 'X', or 'Z').
        void set(uint8_t index, char state)
        {
            if (index >= 64) return; // Out of bounds, do nothing

            switch (state)
            {
                case '0':
                    value &= ~(1ULL << index); // Set value bit to 0
                    mask &= ~(1ULL << index);  // Set mask bit to 0 (definite)
                    break;
                case '1':
                    value |= (1ULL << index);  // Set value bit to 1
                    mask &= ~(1ULL << index);  // Set mask bit to 0 (definite)
                    break;
                case 'X':
                    value &= ~(1ULL << index); // Set value bit to 0
                    mask |= (1ULL << index);   // Set mask bit to 1 (unknown)
                    break;
                case 'Z':
                    value |= (1ULL << index);   // Set value bit to 1
                    mask |= (1ULL << index);    // Set mask bit to 1 (high impedance)
                    break;
                default:
                    // Invalid state, do nothing
                    break;
            }
        }
    };
}

#endif // PULSE_LOGICVECTOR_H