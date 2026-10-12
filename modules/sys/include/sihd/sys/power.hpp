#ifndef __SIHD_SYS_POWER_HPP__
#define __SIHD_SYS_POWER_HPP__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sihd/util/Duration.hpp>

namespace sihd::sys::power
{

enum class BatteryState
{
    charging,
    discharging,
    full,
    not_charging,
    unknown,
};

struct Battery
{
        // BAT0, BAT1, ... / BAT0 on windows
        std::string name;
        std::optional<uint8_t> capacity_percent;
        BatteryState state = BatteryState::unknown;
        std::optional<sihd::util::Duration> time_to_empty;
        std::optional<sihd::util::Duration> time_to_full;
};

struct AcLine
{
        std::string name;
        bool online = false;
};

std::vector<Battery> batteries();
std::vector<AcLine> ac_lines();

} // namespace sihd::sys::power

#endif
