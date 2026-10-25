#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/cap.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/info.hpp>
#include <sihd/sys/power.hpp>
#include <sihd/sys/session.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#if defined(__SIHD_LINUX__)
# include <sihd/sys/linux/cgroup.hpp>
#endif

using namespace sihd::util;
using namespace sihd::sys;

namespace
{

std::string bytes_str(std::optional<uint64_t> bytes)
{
    return bytes.has_value() ? str::bytes_str((int64_t)*bytes) : "n/a";
}

std::string hz_str(std::optional<uint64_t> hz)
{
    if (!hz.has_value())
        return "n/a";
    return fmt::format("{:.2f} GHz", *hz / 1e9);
}

std::string count_str(std::optional<uint32_t> count)
{
    return count.has_value() ? std::to_string(*count) : "n/a";
}

const char *battery_state_str(power::BatteryState state)
{
    switch (state)
    {
        case power::BatteryState::charging:
            return "charging";
        case power::BatteryState::discharging:
            return "discharging";
        case power::BatteryState::full:
            return "full";
        case power::BatteryState::not_charging:
            return "not charging";
        case power::BatteryState::unknown:
            return "unknown";
    }
    return "unknown";
}

const char *session_state_str(session::SessionState state)
{
    switch (state)
    {
        case session::SessionState::active:
            return "active";
        case session::SessionState::connected:
            return "connected";
        case session::SessionState::disconnected:
            return "disconnected";
        case session::SessionState::unknown:
            return "unknown";
    }
    return "unknown";
}

void print_user()
{
    std::vector<std::string> groups;
    for (const user::GroupId & group : user::groups())
        groups.push_back(user::name_of(group).value_or(group.to_string()));

    std::vector<std::string_view> caps;
    for (const Cap cap : cap::all())
    {
        if (cap::has(cap))
            caps.push_back(cap::to_name(cap));
    }

    fmt::print("user:\n");
    fmt::print("  name      : {}\n", user::name().value_or("n/a"));
    fmt::print("  id        : {}\n", user::effective_user().to_string());
    fmt::print("  root      : {}\n", user::is_root());
    fmt::print("  groups    : {}\n", fmt::join(groups, " "));
    fmt::print("  caps      : {}\n", caps.empty() ? "none" : fmt::format("{}", fmt::join(caps, " ")));
}

void print_sessions()
{
    fmt::print("sessions:\n");
    for (const session::Session & session : session::sessions())
    {
        fmt::print("  {:<12} {:<10} {:<12} id={} since {}{}\n",
                   session.username,
                   session.device,
                   session_state_str(session.state),
                   session.id,
                   session.since.get() == 0 ? "unknown" : session.since.local_str(),
                   session.hostname.empty() ? "" : fmt::format(" from {}", session.hostname));
    }
}

void print_power()
{
    fmt::print("power:\n");
    for (const power::AcLine & line : power::ac_lines())
        fmt::print("  {:<10}: {}\n", line.name, line.online ? "online" : "offline");
    for (const power::Battery & battery : power::batteries())
    {
        fmt::print("  {:<10}: {} {}",
                   battery.name,
                   battery.capacity_percent ? fmt::format("{}%", *battery.capacity_percent) : "n/a",
                   battery_state_str(battery.state));
        if (battery.time_to_empty)
            fmt::print(", empty in {}", battery.time_to_empty->str());
        if (battery.time_to_full)
            fmt::print(", full in {}", battery.time_to_full->str());
        fmt::print("\n");
    }
}

#if defined(__SIHD_LINUX__)
std::string memory_limit_str(const Result<std::optional<uint64_t>> & limit)
{
    if (!limit)
        return limit.error().message;
    return limit->has_value() ? bytes_str(**limit) : "max";
}

std::string cpu_limit_str(const Result<linux_::cgroup::CpuLimit> & limit)
{
    if (!limit)
        return limit.error().message;
    if (!limit->quota_us)
        return "max";
    return fmt::format("{:.2f} cpus", static_cast<double>(*limit->quota_us) / static_cast<double>(limit->period_us));
}

void print_cgroup()
{
    const auto memory_current = linux_::cgroup::memory_current();

    fmt::print("cgroup:\n");
    fmt::print("  v2        : {}\n", linux_::cgroup::is_v2_available());
    fmt::print("  group     : {}\n", linux_::cgroup::current_group().value_or("n/a"));
    fmt::print("  container : {}\n", linux_::cgroup::is_container());
    fmt::print("  memory    : {} / {}\n",
               memory_current ? bytes_str(*memory_current) : memory_current.error().message,
               memory_limit_str(linux_::cgroup::memory_max()));
    fmt::print("  cpu       : {}\n", cpu_limit_str(linux_::cgroup::cpu_max()));
}
#endif

} // namespace

int main(int argc, char **argv)
{
    App app({
        .name = "sys_info",
        .description = "Dump system information",
    });

    app.root().on_run([] {
        fmt::print("memory:\n");
        fmt::print("  total     : {}\n", bytes_str(info::total_ram()));
        fmt::print("  available : {}\n", bytes_str(info::available_ram()));
        fmt::print("  swap      : {} free / {}\n", bytes_str(info::free_swap()), bytes_str(info::total_swap()));

        fmt::print("cpu:\n");
        fmt::print("  cores     : {} physical / {} logical\n",
                   count_str(info::physical_core_count()),
                   info::cpu_count());
        fmt::print("  frequency : {}\n", hz_str(info::cpu_frequency()));
        fmt::print("  max       : {}\n", hz_str(info::cpu_max_frequency()));

        info::CpuUsage usage;
        time::msleep(200);
        usage.sample();
        fmt::print("  usage     : {:.1f}%\n", usage.usage());
        fmt::print("  per core  :");
        for (double core_usage : usage.per_core_usage())
            fmt::print(" {:.0f}%", core_usage);
        fmt::print("\n");

        const info::OsInfo os = info::os_info();
        fmt::print("system:\n");
        fmt::print("  name      : {}\n", os.name);
        fmt::print("  version   : {}\n", os.version);
        fmt::print("  kernel    : {}\n", os.kernel);
        fmt::print("  arch      : {}\n", os.arch);
        fmt::print("  hostname  : {}\n", info::hostname());
        fmt::print("  domain    : {}\n", info::domain().value_or(""));
        fmt::print("  uptime    : {}\n", info::uptime().str());
        const auto load = info::load_average();
        fmt::print("  load      : {:.2f} {:.2f} {:.2f}\n", load[0], load[1], load[2]);

        const info::HardwareIdentity hw = info::hardware_identity();
        fmt::print("hardware:\n");
        fmt::print("  vendor    : {}\n", hw.vendor);
        fmt::print("  model     : {}\n", hw.model);
        fmt::print("  serial    : {}\n", hw.serial);

        print_user();
        print_sessions();
        print_power();
#if defined(__SIHD_LINUX__)
        print_cgroup();
#endif

        fmt::print("mounts:\n");
        for (const auto & mount : fs::mounts())
        {
            fmt::print("  {:<24} {:<8} {} free / {}\n",
                       mount.mount_point,
                       mount.fs_type,
                       bytes_str(fs::free_space(mount.mount_point).value_or(0)),
                       bytes_str(fs::total_space(mount.mount_point).value_or(0)));
        }
    });

    return app.run(argc, argv);
}
