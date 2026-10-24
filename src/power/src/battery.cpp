#include "we/power/battery.hpp"

#include <algorithm>
#include <fstream>
#include <optional>

namespace we::power {
namespace {

std::optional<std::string> read_first_line (const std::filesystem::path& path) {
    std::ifstream stream (path);

    if (!stream) {
        return std::nullopt;
    }

    std::string value;
    std::getline (stream, value);
    return value;
}

} // namespace

BatteryMonitor::BatteryMonitor (std::filesystem::path power_supply_root) : root_ (std::move (power_supply_root)) {
    std::error_code error;
    std::filesystem::directory_iterator iterator (this->root_, error);

    if (error) {
        return;
    }

    for (const auto& entry : iterator) {
        if (!entry.is_directory ()) {
            continue;
        }

        const std::string type = read_first_line (entry.path () / "type").value_or ("");

        if (type == "Battery") {
            this->batteries_.push_back (
                Battery { .capacity = entry.path () / "capacity", .status = entry.path () / "status" }
            );
        } else if (type == "Mains" || type == "AC" || type == "USB") {
            this->has_ac_ = true;
        }
    }

    this->has_battery_ = !this->batteries_.empty ();
}

PowerStatus BatteryMonitor::poll () const {
    PowerStatus status;

    if (!this->has_battery_) {
        status.source = PowerSource::ac;
        return status;
    }

    int capacity_sum = 0;
    int capacity_count = 0;

    for (const auto& battery : this->batteries_) {
        const auto capacity = read_first_line (battery.capacity);
        const auto state = read_first_line (battery.status);

        if (capacity.has_value ()) {
            try {
                capacity_sum += std::stoi (*capacity);
                ++capacity_count;
            } catch (const std::exception&) {
                // Ignore malformed sysfs values; another battery may be valid.
            }
        }

        if (state.has_value ()) {
            if (*state == "Charging") {
                status.charging = true;
            } else if (*state == "Discharging") {
                status.discharging = true;
            }
        }
    }

    if (capacity_count > 0) {
        status.battery_percent = capacity_sum / capacity_count;
    }

    if (status.discharging) {
        status.source = PowerSource::battery;
    } else if (status.charging || this->has_ac_) {
        status.source = PowerSource::ac;
    } else {
        status.source = PowerSource::battery;
    }

    return status;
}

} // namespace we::power
