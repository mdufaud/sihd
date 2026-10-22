#include <algorithm>
#include <initializer_list>
#include <utility>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/power.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;

namespace sihd::sys::power
{

SIHD_NEW_LOGGER("sihd::sys::power");

namespace
{

constexpr const char *power_supply_dir = "/sys/class/power_supply";

std::optional<std::string> read_str(const std::string & path)
{
    const auto content = fs::read_all(path);
    if (!content)
        return std::nullopt;
    return std::string(str::trim(*content));
}

std::optional<uint64_t> read_uint(const std::string & path)
{
    const auto content = fs::read_all(path);
    if (!content)
        return std::nullopt;
    const auto parsed = str::convert_from_string<uint64_t>(str::trim(*content));
    if (!parsed)
        return std::nullopt;
    return *parsed;
}

std::optional<uint64_t> first_uint(std::initializer_list<std::string> paths)
{
    for (const std::string & path : paths)
    {
        if (const auto value = read_uint(path))
            return value;
    }
    return std::nullopt;
}

BatteryState parse_state(std::string_view status)
{
    if (status == "Charging")
        return BatteryState::charging;
    if (status == "Discharging")
        return BatteryState::discharging;
    if (status == "Full")
        return BatteryState::full;
    if (status == "Not charging")
        return BatteryState::not_charging;
    return BatteryState::unknown;
}

// sysfs ratios are uWh/uW or uAh/uA - both resolve to hours
std::optional<Duration> units_to_time(std::optional<uint64_t> units, std::optional<uint64_t> units_per_hour)
{
    if (!units || !units_per_hour || *units_per_hour == 0)
        return std::nullopt;
    return Duration(time::from_double((double)*units / (double)*units_per_hour * 3600.0));
}

std::vector<std::pair<std::string, std::string>> power_supplies(std::initializer_list<std::string_view> types)
{
    std::vector<std::pair<std::string, std::string>> ret;
    for (const std::string & child : fs::children(power_supply_dir))
    {
        // children() appends the separator to directories
        std::string_view name = child;
        if (!name.empty() && name.back() == '/')
            name.remove_suffix(1);
        const std::string dir = fs::combine(power_supply_dir, name);
        const std::optional<std::string> type = read_str(fs::combine(dir, "type"));
        if (!type)
            continue;
        for (std::string_view wanted : types)
        {
            if (*type == wanted)
            {
                ret.emplace_back(dir, name);
                break;
            }
        }
    }
    return ret;
}

std::optional<Battery> read_battery(std::string_view dir, std::string_view name)
{
    if (read_uint(fs::combine(dir, "present")).value_or(1) == 0)
        return std::nullopt;
    Battery battery;
    battery.name = name;
    const std::optional<std::string> status = read_str(fs::combine(dir, "status"));
    battery.state = status ? parse_state(*status) : BatteryState::unknown;
    const std::optional<uint64_t> now = first_uint({fs::combine(dir, "energy_now"), fs::combine(dir, "charge_now")});
    const std::optional<uint64_t> full = first_uint({fs::combine(dir, "energy_full"), fs::combine(dir, "charge_full")});
    const std::optional<uint64_t> rate = first_uint({fs::combine(dir, "power_now"), fs::combine(dir, "current_now")});
    const std::optional<uint64_t> capacity = read_uint(fs::combine(dir, "capacity"));
    if (capacity)
        battery.capacity_percent = (uint8_t)std::min<uint64_t>(100, *capacity);
    else if (now && full && *full > 0)
        battery.capacity_percent = (uint8_t)std::min<uint64_t>(100, *now * 100 / *full);
    switch (battery.state)
    {
        case BatteryState::discharging:
            battery.time_to_empty = units_to_time(now, rate);
            break;
        case BatteryState::charging:
            if (full && now && *full > *now)
                battery.time_to_full = units_to_time(*full - *now, rate);
            break;
        default:
            break;
    }
    return battery;
}

} // namespace

std::vector<Battery> batteries()
{
    std::vector<Battery> ret;
    for (const auto & [dir, name] : power_supplies({"Battery"}))
    {
        if (auto battery = read_battery(dir, name))
            ret.push_back(std::move(*battery));
    }
    return ret;
}

std::vector<AcLine> ac_lines()
{
    std::vector<AcLine> ret;
    for (const auto & [dir, name] : power_supplies({"Mains", "USB"}))
    {
        AcLine line;
        line.name = name;
        line.online = read_uint(fs::combine(dir, "online")).value_or(0) != 0;
        ret.push_back(std::move(line));
    }
    return ret;
}

} // namespace sihd::sys::power
