#include "../internal/info.hpp"

#include <sys/utsname.h>
#include <unistd.h> // sysconf gethostname getdomainname

#include <cctype> // isdigit
#include <cstring>
#include <set>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/info.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

namespace sihd::sys::info
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::info");

namespace
{

std::optional<uint64_t> read_uint(std::string_view path)
{
    auto content_opt = fs::read_all(path);
    if (!content_opt)
        return std::nullopt;
    return str::convert_from_string<uint64_t>(str::trim(*content_opt));
}

// /proc/meminfo values are kibibytes
std::optional<uint64_t> meminfo_kib(std::string_view key)
{
    const auto lines = fs::read_lines("/proc/meminfo");
    if (!lines)
        return std::nullopt;
    for (const auto & line : *lines)
    {
        const auto [name, value] = str::split_pair_view(line, ":");
        if (str::trim(name) != key)
            continue;
        std::string_view kib = str::trim(value);
        kib = kib.substr(0, kib.find(' '));
        const auto v = str::convert_from_string<uint64_t>(kib);
        return v ? std::optional<uint64_t>(*v * 1024) : std::nullopt;
    }
    return std::nullopt;
}

std::optional<CpuTimes> parse_cpu_times(std::string_view line)
{
    const auto fields = str::split(line);
    // "cpu" label followed by user nice system idle iowait irq softirq steal;
    // guest times are already counted in user/nice
    if (fields.size() < 9)
        return std::nullopt;
    static const long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0)
        return std::nullopt;
    CpuTimes times;
    uint64_t *values[] =
        {&times.user, &times.nice, &times.system, &times.idle, &times.iowait, &times.irq, &times.softirq, &times.steal};
    for (size_t i = 0; i < 8; ++i)
    {
        const auto ticks = str::convert_from_string<uint64_t>(fields[i + 1]);
        if (!ticks)
            return std::nullopt;
        *values[i] = *ticks * 1'000'000'000ull / (uint64_t)clk_tck;
    }
    return times;
}

std::vector<std::string> cpu_sysfs_dirs()
{
    std::vector<std::string> dirs;
    for (const std::string & name : fs::children("/sys/devices/system/cpu"))
    {
        // children() appends the separator to directories
        std::string_view base = name;
        if (!base.empty() && base.back() == '/')
            base.remove_suffix(1);
        if (base.size() > 3 && base.starts_with("cpu") && isdigit((unsigned char)base[3]))
            dirs.push_back(fs::combine("/sys/devices/system/cpu", base));
    }
    return dirs;
}

// file is a cpufreq file name relative to a cpu sysfs dir, in kHz
std::optional<uint64_t> max_khz_over_cpus(std::string_view file)
{
    std::optional<uint64_t> hz;
    for (const std::string & dir : cpu_sysfs_dirs())
    {
        const auto khz = read_uint(fs::combine(dir, file));
        if (khz && *khz > 0)
            hz = std::max(hz.value_or(0), *khz * 1000);
    }
    return hz;
}

std::optional<uint64_t> cpuinfo_mhz_to_hz()
{
    std::optional<uint64_t> hz;
    const auto lines = fs::read_lines("/proc/cpuinfo");
    if (!lines)
        return hz;
    for (const auto & line : *lines)
    {
        const auto [name, value] = str::split_pair_view(line, ":");
        if (str::trim(name) != "cpu MHz")
            continue;
        if (const auto mhz = str::convert_from_string<double>(str::trim(value)))
            hz = std::max(hz.value_or(0), (uint64_t)(*mhz * 1'000'000.0));
    }
    return hz;
}

std::string dmi_string(std::string_view name)
{
    auto content_opt = fs::read_all(fs::combine("/sys/class/dmi/id", name));
    return content_opt ? std::string(str::trim(*content_opt)) : std::string {};
}

} // namespace

std::optional<uint64_t> total_ram()
{
    return meminfo_kib("MemTotal");
}

std::optional<uint64_t> available_ram()
{
    return meminfo_kib("MemAvailable");
}

std::optional<uint64_t> total_swap()
{
    return meminfo_kib("SwapTotal");
}

std::optional<uint64_t> free_swap()
{
    return meminfo_kib("SwapFree");
}

uint32_t cpu_count()
{
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (uint32_t)count : 0;
}

std::optional<uint32_t> physical_core_count()
{
    // str::split drops empty lines so processor blocks cannot be delimited by them;
    // a block starts with the "processor" entry (or a "physical id" one)
    auto content_opt = fs::read_all("/proc/cpuinfo");
    if (content_opt)
    {
        std::set<std::pair<uint64_t, uint64_t>> cores;
        std::optional<uint64_t> physical_id;
        std::optional<uint64_t> core_id;
        const auto flush = [&] {
            if (physical_id && core_id)
                cores.emplace(*physical_id, *core_id);
            physical_id.reset();
            core_id.reset();
        };
        for (const auto & line : str::split(*content_opt, '\n'))
        {
            const auto [name, value] = str::split_pair_view(line, ":");
            const std::string_view key = str::trim(name);
            if (key == "processor" || key == "physical id")
                flush();
            if (key == "physical id")
                physical_id = str::convert_from_string<uint64_t>(str::trim(value));
            else if (key == "core id")
                core_id = str::convert_from_string<uint64_t>(str::trim(value));
        }
        flush();
        if (!cores.empty())
            return (uint32_t)cores.size();
    }
    // architectures without "physical id"/"core id" in /proc/cpuinfo (arm64...)
    std::set<std::pair<uint64_t, uint64_t>> cores;
    for (const std::string & dir : cpu_sysfs_dirs())
    {
        const auto package = read_uint(fs::combine({dir, "topology", "physical_package_id"}));
        const auto core = read_uint(fs::combine({dir, "topology", "core_id"}));
        if (package && core)
            cores.emplace(*package, *core);
    }
    if (cores.empty())
        return std::nullopt;
    return (uint32_t)cores.size();
}

std::optional<CpuTimes> cpu_times()
{
    const auto lines = fs::read_lines("/proc/stat");
    if (!lines)
        return std::nullopt;
    for (const auto & line : *lines)
    {
        if (line.starts_with("cpu "))
            return parse_cpu_times(line);
    }
    return std::nullopt;
}

std::vector<CpuTimes> per_core_cpu_times()
{
    std::vector<CpuTimes> ret;
    const auto lines = fs::read_lines("/proc/stat");
    if (!lines)
        return ret;
    for (const auto & line : *lines)
    {
        if (line.size() > 3 && line.starts_with("cpu") && isdigit((unsigned char)line[3]))
        {
            if (const auto times = parse_cpu_times(line))
                ret.push_back(*times);
        }
    }
    return ret;
}

std::optional<uint64_t> cpu_frequency()
{
    if (const auto hz = max_khz_over_cpus("cpufreq/scaling_cur_freq"))
        return hz;
    return cpuinfo_mhz_to_hz();
}

std::optional<uint64_t> cpu_max_frequency()
{
    if (const auto hz = max_khz_over_cpus("cpufreq/cpuinfo_max_freq"))
        return hz;
    return max_khz_over_cpus("cpufreq/scaling_max_freq");
}

OsInfo os_info()
{
    OsInfo info;
    struct utsname uts;
    if (uname(&uts) == 0)
    {
        info.kernel = uts.release;
        info.arch = uts.machine;
    }
    const auto lines = fs::read_lines("/etc/os-release");
    if (lines)
    {
        for (const auto & line : *lines)
        {
            const auto [key, value] = str::split_pair_view(line, "=");
            const std::string_view k = str::trim(key);
            if (k == "NAME")
                info.name = str::unquote(value);
            else if (k == "VERSION_ID")
                info.version = str::unquote(value);
        }
    }
    return info;
}

std::string hostname()
{
    char buf[256];
    if (gethostname(buf, sizeof(buf) - 1) != 0)
        return "";
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

std::optional<std::string> domain()
{
#if defined(__SIHD_ANDROID__)
    return std::nullopt;
#else
    char buf[256];
    if (getdomainname(buf, sizeof(buf) - 1) != 0)
        return std::nullopt;
    buf[sizeof(buf) - 1] = '\0';
    // no NIS domain configured
    if (buf[0] == '\0' || strcmp(buf, "(none)") == 0)
        return std::nullopt;
    return std::string(buf);
#endif
}

Duration uptime()
{
    const auto lines = fs::read_lines("/proc/uptime");
    if (!lines || lines->empty())
        return Duration {};
    const auto parts = str::split_pair_view(str::trim((*lines)[0]), " ");
    const auto secs = str::convert_from_string<double>(parts.first);
    if (!secs)
        return Duration {};
    return Duration(std::chrono::nanoseconds((int64_t)(*secs * 1e9)));
}

std::array<double, 3> load_average()
{
    const auto lines = fs::read_lines("/proc/loadavg");
    if (!lines || lines->empty())
        return {0.0, 0.0, 0.0};
    const auto fields = str::split((*lines)[0]);
    if (fields.size() < 3)
        return {0.0, 0.0, 0.0};
    std::array<double, 3> ret {};
    for (size_t i = 0; i < 3; ++i)
    {
        if (const auto v = str::convert_from_string<double>(fields[i]))
            ret[i] = *v;
    }
    return ret;
}

HardwareIdentity hardware_identity()
{
    HardwareIdentity id;
    id.vendor = internal::dmi_identity_field(dmi_string("sys_vendor"), dmi_string("board_vendor"));
    id.model = internal::dmi_identity_field(dmi_string("product_name"), dmi_string("board_name"));
    id.serial = internal::dmi_identity_field(dmi_string("product_serial"), dmi_string("board_serial"));
    return id;
}

} // namespace sihd::sys::info
