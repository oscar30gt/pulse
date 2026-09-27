#ifndef PULSE_SHARED_CHECKED_MATH_H
#define PULSE_SHARED_CHECKED_MATH_H

// Overflow-checked 64-bit arithmetic: constant folding (analyzer, elaborator) and unit definitions give up
// (nullopt) or report a diagnostic instead of silently wrapping around.

#include <cstdint>
#include <limits>
#include <optional>

namespace Pulse
{
    constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
    constexpr int64_t kInt64Min = std::numeric_limits<int64_t>::min();

    inline std::optional<int64_t> checkedAdd(int64_t a, int64_t b)
    {
        if ((b > 0 && a > kInt64Max - b) || (b < 0 && a < kInt64Min - b)) return std::nullopt;
        return a + b;
    }

    inline std::optional<int64_t> checkedSub(int64_t a, int64_t b)
    {
        if ((b < 0 && a > kInt64Max + b) || (b > 0 && a < kInt64Min + b)) return std::nullopt;
        return a - b;
    }

    inline std::optional<int64_t> checkedMul(int64_t a, int64_t b)
    {
        // Checked before multiplying: signed overflow is undefined, so an optimizer may drop a test made on the product.
        if (a == 0 || b == 0) return 0;
        const bool overflows = a > 0 ? (b > 0 ? a > kInt64Max / b : b < kInt64Min / a)
                                     : (b > 0 ? a < kInt64Min / b : b < kInt64Max / a);
        if (overflows) return std::nullopt;
        return a * b;
    }

    /// base ** exponent for a non-negative exponent; trivial bases never loop.
    inline std::optional<int64_t> checkedPower(int64_t base, int64_t exponent)
    {
        if (exponent < 0) return std::nullopt;
        if (exponent == 0 || base == 1) return 1;
        if (base == 0) return 0;
        if (base == -1) return exponent % 2 == 0 ? 1 : -1;

        int64_t result = 1;
        for (int64_t i = 0; i < exponent; ++i)
        {
            auto next = checkedMul(result, base);
            if (!next) return std::nullopt;
            result = *next;
        }
        return result;
    }

} // namespace Pulse

#endif // PULSE_SHARED_CHECKED_MATH_H
