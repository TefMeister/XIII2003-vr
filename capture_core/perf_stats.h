// capture_core/perf_stats.h
//
// Small pure helpers for the capture-path performance work: a wrap-safe
// millisecond rate limiter (to cap GPU->CPU readbacks below game fps) and a
// per-phase timing accumulator (feeds the [xiii-perf] DebugView heartbeat).
// No Windows dependencies; callers supply tick/time values.

#pragma once
#include <cstdint>

namespace xiii {

// Allows an action at most once per `intervalMs`. Tick math is unsigned
// 32-bit, so GetTickCount wraparound (49.7 days) is handled naturally.
// interval 0 means "always allow".
class RateLimiter {
public:
    explicit RateLimiter(uint32_t intervalMs);
    // True if the interval has elapsed since the last allowed call (or this is
    // the first call). An allowed call becomes the new reference point;
    // denied calls do not.
    bool Allow(uint32_t nowMs);

private:
    uint32_t interval_;
    uint32_t last_;
    bool     any_;
};

// Accumulates duration samples (microseconds) for one measured phase between
// heartbeat reports: count, average, and worst case.
struct PhaseStats {
    uint32_t count   = 0;
    uint64_t totalUs = 0;
    uint32_t maxUs   = 0;

    void     Add(uint32_t us);
    uint32_t AvgUs() const;  // 0 when no samples
    void     Reset();
};

}  // namespace xiii
