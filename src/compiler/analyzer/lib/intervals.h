#ifndef PULSE_COMPILER_INTERVALS_H
#define PULSE_COMPILER_INTERVALS_H

// Sets of integer intervals, used to check that aggregate indices and case choices are
// covered exactly once.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace Pulse::Parser
{
    struct Interval
    {
        int64_t low = 0;
        int64_t high = 0;
    };

    /// Sum of the sizes of the intervals (they must not overlap).
    inline int64_t totalLength(const std::vector<Interval>& intervals)
    {
        int64_t total = 0;
        for (const Interval& i : intervals) total += i.high - i.low + 1;
        return total;
    }

    /// Sorts the intervals and returns the first value covered twice, if any.
    inline std::optional<int64_t> findOverlap(std::vector<Interval>& intervals)
    {
        std::sort(intervals.begin(), intervals.end(), [](const Interval& a, const Interval& b) { return a.low < b.low; });
        for (size_t i = 1; i < intervals.size(); ++i)
            if (intervals[i].low <= intervals[i - 1].high)
                return intervals[i].low;
        return std::nullopt;
    }

    /// First value of [low, high] that no (sorted, non-overlapping) interval covers.
    inline std::optional<int64_t> firstGap(const std::vector<Interval>& sorted, int64_t low, int64_t high)
    {
        int64_t next = low;
        for (const Interval& i : sorted)
        {
            if (i.low > next) break;
            next = std::max(next, i.high + 1);
        }
        return next <= high ? std::optional<int64_t>(next) : std::nullopt;
    }

} // namespace Pulse::Parser

#endif // PULSE_COMPILER_INTERVALS_H
