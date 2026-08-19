// capture_core/perf_stats.cpp
#include "perf_stats.h"

namespace xiii {

RateLimiter::RateLimiter(uint32_t intervalMs)
    : interval_(intervalMs), last_(0), any_(false) {}

bool RateLimiter::Allow(uint32_t nowMs) {
    // Unsigned subtraction is wraparound-correct for tick counters.
    if (any_ && (uint32_t)(nowMs - last_) < interval_) return false;
    any_ = true;
    last_ = nowMs;
    return true;
}

void PhaseStats::Add(uint32_t us) {
    ++count;
    totalUs += us;
    if (us > maxUs) maxUs = us;
}

uint32_t PhaseStats::AvgUs() const {
    return count ? (uint32_t)(totalUs / count) : 0;
}

void PhaseStats::Reset() {
    count = 0;
    totalUs = 0;
    maxUs = 0;
}

}  // namespace xiii
