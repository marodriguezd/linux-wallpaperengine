#pragma once

#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>

namespace we::core {

enum class LogLevel { debug, info, warning, error };

namespace detail {
inline std::mutex& log_mutex () {
    static std::mutex mutex;
    return mutex;
}

inline bool& verbose_logging () {
    static bool verbose = false;
    return verbose;
}
} // namespace detail

inline void set_verbose_logging (bool enabled) noexcept { detail::verbose_logging () = enabled; }

inline void log (LogLevel level, std::string_view message) noexcept {
    if (level == LogLevel::debug && !detail::verbose_logging ()) {
        return;
    }

    const char* prefix = "info";
    switch (level) {
        case LogLevel::debug:
            prefix = "debug";
            break;
        case LogLevel::info:
            prefix = "info";
            break;
        case LogLevel::warning:
            prefix = "warn";
            break;
        case LogLevel::error:
            prefix = "error";
            break;
    }

    const std::lock_guard lock (detail::log_mutex ());
    std::fprintf (stderr, "we: %s: %.*s\n", prefix, static_cast<int> (message.size ()), message.data ());
    std::fflush (stderr);
}

namespace detail {
template <typename Part> void append_part (std::string& message, Part&& part) {
    using Value = std::remove_cvref_t<Part>;

    if constexpr (std::is_convertible_v<Value, std::string_view>) {
        message.append (std::string_view (part));
    } else if constexpr (std::is_arithmetic_v<Value>) {
        message.append (std::to_string (part));
    } else {
        static_assert (sizeof (Value) == 0, "log parts must be strings or numbers");
    }
}
} // namespace detail

template <typename... Parts> void log_joined (LogLevel level, Parts&&... parts) {
    if (level == LogLevel::debug && !detail::verbose_logging ()) {
        return;
    }

    std::string message;
    (detail::append_part (message, parts), ...);
    log (level, message);
}

template <typename... Parts> void log_debug (Parts&&... parts) { log_joined (LogLevel::debug, std::forward<Parts> (parts)...); }
template <typename... Parts> void log_info (Parts&&... parts) { log_joined (LogLevel::info, std::forward<Parts> (parts)...); }
template <typename... Parts> void log_warn (Parts&&... parts) { log_joined (LogLevel::warning, std::forward<Parts> (parts)...); }
template <typename... Parts> void log_error (Parts&&... parts) { log_joined (LogLevel::error, std::forward<Parts> (parts)...); }

} // namespace we::core
