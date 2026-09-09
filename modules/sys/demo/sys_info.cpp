#include <fmt/format.h>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/info.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#include <CLI/CLI.hpp>

using namespace sihd::sys;

namespace
{

std::string bytes_str(std::optional<uint64_t> bytes)
{
    return bytes.has_value() ? sihd::util::str::bytes_str((int64_t)*bytes) : "n/a";
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

} // namespace

int main(int argc, char **argv)
{
    CLI::App app {"Dump system information"};
    CLI11_PARSE(app, argc, argv);

    fmt::print("memory:\n");
    fmt::print("  total     : {}\n", bytes_str(info::total_ram()));
    fmt::print("  available : {}\n", bytes_str(info::available_ram()));
    fmt::print("  swap      : {} free / {}\n", bytes_str(info::free_swap()), bytes_str(info::total_swap()));

    fmt::print("cpu:\n");
    fmt::print("  cores     : {} physical / {} logical\n", count_str(info::physical_core_count()), info::cpu_count());
    fmt::print("  frequency : {}\n", hz_str(info::cpu_frequency()));
    fmt::print("  max       : {}\n", hz_str(info::cpu_max_frequency()));

    info::CpuUsage usage;
    sihd::util::time::msleep(200);
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

    fmt::print("mounts:\n");
    for (const auto & mount : fs::mounts())
    {
        fmt::print("  {:<24} {:<8} {} free / {}\n",
                   mount.mount_point,
                   mount.fs_type,
                   bytes_str(fs::free_space(mount.mount_point)),
                   bytes_str(fs::total_space(mount.mount_point)));
    }

    return 0;
}
