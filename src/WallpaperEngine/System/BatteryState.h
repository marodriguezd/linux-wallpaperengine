#pragma once

#include <chrono>

namespace WallpaperEngine::System {
/**
 * Battery status with UPower (system bus) as primary source and
 * /sys/class/power_supply as fallback. Results are cached for a few
 * seconds so it is cheap to query every frame. Never throws.
 */
class BatteryState {
public:
    /** @return true if the system is currently running on battery */
    bool isOnBattery ();

private:
    /** @return true if UPower answered, result in out */
    bool queryUPower (bool& out);
    /** @return true if any Battery supply reports Discharging */
    bool querySysfs ();

    std::chrono::steady_clock::time_point m_lastCheck {};
    bool m_cached = false;
    bool m_haveCache = false;
};
} // namespace WallpaperEngine::System
