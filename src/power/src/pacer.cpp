#include "we/power/pacer.hpp"

#include <algorithm>
#include <cerrno>
#include <ctime>

namespace we::power {
namespace {

std::int64_t now_ns () {
    timespec value {};
    clock_gettime (CLOCK_MONOTONIC, &value);
    return static_cast<std::int64_t> (value.tv_sec) * 1'000'000'000LL + value.tv_nsec;
}

timespec to_timespec (std::int64_t nanoseconds) {
    timespec value {};
    value.tv_sec = static_cast<time_t> (nanoseconds / 1'000'000'000LL);
    value.tv_nsec = static_cast<long> (nanoseconds % 1'000'000'000LL);
    return value;
}

} // namespace

FramePacer::FramePacer (double frames_per_second) noexcept : rate_ (frames_per_second) {
    this->reset ();
}

void FramePacer::set_rate (double frames_per_second) noexcept {
    const double clamped = std::clamp (frames_per_second, 0.0, 1000.0);

    if (clamped != this->rate_) {
        this->rate_ = clamped;
        this->reset ();
    }
}

void FramePacer::reset () noexcept {
    this->next_ns_ = now_ns ();
    this->frames_ = 0;
}

void FramePacer::wait () {
    if (this->rate_ <= 0.0) {
        return;
    }

    const auto period = static_cast<std::int64_t> (1'000'000'000.0 / this->rate_);
    this->next_ns_ += period;

    const std::int64_t current = now_ns ();

    // If rendering fell more than a second behind, resynchronize instead of
    // trying to catch up with a burst of frames.
    if (current - this->next_ns_ > 1'000'000'000LL) {
        this->next_ns_ = current + period;
    }

    if (this->next_ns_ > current) {
        const timespec deadline = to_timespec (this->next_ns_);

        while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, nullptr) == EINTR) {
        }
    }

    ++this->frames_;
}

} // namespace we::power
