// clang-format off
# include <windows.h>
// clang-format on

#include <utility>
#include <vector>

#include <sihd/sys/power.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;

namespace sihd::sys::power
{

SIHD_NEW_LOGGER("sihd::sys::power");

namespace
{

bool system_power_status(SYSTEM_POWER_STATUS & status)
{
    return GetSystemPowerStatus(&status) != 0;
}

} // namespace

std::vector<Battery> batteries()
{
    std::vector<Battery> ret;
    SYSTEM_POWER_STATUS status;
    if (!system_power_status(status))
        return ret;
    if (status.BatteryFlag == BATTERY_FLAG_NO_BATTERY || status.BatteryFlag == BATTERY_FLAG_UNKNOWN)
        return ret;
    Battery battery;

    battery.name = "BAT0";
    // BatteryLifePercent is 255 when unknown
    if (status.BatteryLifePercent <= 100)
        battery.capacity_percent = status.BatteryLifePercent;
    if (status.BatteryFlag & BATTERY_FLAG_CHARGING)
        battery.state = BatteryState::charging;
    else if (status.ACLineStatus == 1)
        battery.state = status.BatteryLifePercent == 100 ? BatteryState::full : BatteryState::not_charging;
    else if (status.ACLineStatus == 0)
        battery.state = BatteryState::discharging;
    // seconds, (DWORD)-1 when unknown
    if (status.BatteryLifeTime != (DWORD)-1)
        battery.time_to_empty = Duration(std::chrono::seconds(status.BatteryLifeTime));
    ret.push_back(std::move(battery));
    return ret;
}

std::vector<AcLine> ac_lines()
{
    std::vector<AcLine> ret;
    SYSTEM_POWER_STATUS status;
    if (!system_power_status(status))
        return ret;
    // 255 = unknown
    if (status.ACLineStatus != 0 && status.ACLineStatus != 1)
        return ret;
    AcLine line;
    line.name = "AC";
    line.online = status.ACLineStatus == 1;
    ret.push_back(std::move(line));
    return ret;
}

} // namespace sihd::sys::power
