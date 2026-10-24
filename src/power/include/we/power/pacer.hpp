#pragma once

#include <cstdint>

namespace we::power {

// Sleeps until the next frame deadline using CLOCK_MONOTONIC absolute timers.
// No busy waiting; late frames reset the schedule instead of bursting.
class FramePacer {
public:
    explicit FramePacer (double frames_per_second = 60.0) noexcept;

    void set_rate (double frames_per_second) noexcept;
    void reset () noexcept;

    void wait ();

    [[nodiscard]] double rate () const noexcept { return this->rate_; }
    [[nodiscard]] std::uint64_t frames () const noexcept { return this->frames_; }

private:
    double rate_ = 60.0;
    std::int64_t next_ns_ = 0;
    std::uint64_t frames_ = 0;
};

} // namespace we::power
