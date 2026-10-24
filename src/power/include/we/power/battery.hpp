#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace we::power {

enum class PowerSource : std::uint8_t { unknown, ac, battery };

struct PowerStatus {
    PowerSource source = PowerSource::unknown;
    int battery_percent = -1;
    bool charging = false;
    bool discharging = false;
};

// Reads /sys/class/power_supply directly. This is intentionally dependency-free
// and works on every Linux system with a battery; UPower is not required.
class BatteryMonitor {
public:
    explicit BatteryMonitor (std::filesystem::path power_supply_root = "/sys/class/power_supply");

    [[nodiscard]] PowerStatus poll () const;

    [[nodiscard]] bool available () const noexcept { return this->has_battery_; }

private:
    struct Battery {
        std::filesystem::path capacity;
        std::filesystem::path status;
    };

    std::filesystem::path root_;
    std::vector<Battery> batteries_;
    bool has_ac_ = false;
    bool has_battery_ = false;
};

} // namespace we::power
