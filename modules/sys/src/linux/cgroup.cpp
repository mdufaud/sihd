#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <array>
# include <string_view>
# include <vector>

# include <sihd/sys/fs.hpp>
# include <sihd/sys/linux/cgroup.hpp>
# include <sihd/util/str.hpp>

# include "internal/sysfs.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::linux_::cgroup
{

namespace
{

constexpr std::string_view cgroup_root = "/sys/fs/cgroup";
constexpr std::string_view unified_prefix = "0::";
constexpr std::array<std::string_view, 4> container_tokens = {"docker", "containerd", "kubepods", "lxc"};

Result<std::string> group_file(std::string_view name)
{
    if constexpr (!supported)
        return std::unexpected(Error(not_supported, "not supported on this platform"));
    auto path = current_group_path();
    if (!path)
        return std::unexpected(Error(not_found, "no cgroup v2 group"));
    return fs::combine({*path, name});
}

Result<std::optional<uint64_t>> parse_max(std::string_view value)
{
    if (value == "max")
        return std::nullopt;
    auto parsed = str::to_unsigned(value, 10);
    SIHD_UNEXPECTED_RETURN(parsed);
    return static_cast<uint64_t>(*parsed);
}

std::string max_str(std::optional<uint64_t> value)
{
    return value ? std::to_string(*value) : "max";
}

} // namespace

bool is_v2_available()
{
    return fs::exists(fs::combine({cgroup_root, "cgroup.controllers"}));
}

std::optional<std::string> current_group()
{
    auto content = fs::read_all("/proc/self/cgroup");
    if (!content)
        return std::nullopt;
    for (std::string_view line : str::split(*content, '\n'))
    {
        if (line.starts_with(unified_prefix))
            return std::string(str::trim(line.substr(unified_prefix.size())));
    }
    return std::nullopt;
}

std::optional<std::string> current_group_path()
{
    auto group = current_group();
    if (!group || !is_v2_available())
        return std::nullopt;
    return *group == "/" ? std::string(cgroup_root) : std::string(cgroup_root) + *group;
}

bool is_container()
{
    if (fs::exists("/.dockerenv") || fs::exists("/run/.containerenv"))
        return true;
    auto content = fs::read_all("/proc/self/cgroup");
    if (!content)
        return false;
    for (std::string_view token : container_tokens)
    {
        if (content->find(token) != std::string::npos)
            return true;
    }
    return false;
}

Result<std::optional<uint64_t>> memory_max()
{
    auto path = group_file("memory.max");
    SIHD_UNEXPECTED_RETURN(path);
    auto value = internal::sysfs_read(*path);
    SIHD_UNEXPECTED_RETURN(value);
    return parse_max(*value);
}

Result<uint64_t> memory_current()
{
    auto path = group_file("memory.current");
    SIHD_UNEXPECTED_RETURN(path);
    auto value = internal::sysfs_read(*path);
    SIHD_UNEXPECTED_RETURN(value);
    auto parsed = str::to_unsigned(*value, 10);
    SIHD_UNEXPECTED_RETURN(parsed);
    return static_cast<uint64_t>(*parsed);
}

Result<CpuLimit> cpu_max()
{
    auto path = group_file("cpu.max");
    SIHD_UNEXPECTED_RETURN(path);
    auto value = internal::sysfs_read(*path);
    SIHD_UNEXPECTED_RETURN(value);

    const std::vector<std::string> fields = str::split(*value, ' ');
    if (fields.size() != 2)
        return std::unexpected(Error(invalid_argument, "malformed cpu.max: '{}'", *value));
    auto quota = parse_max(fields[0]);
    SIHD_UNEXPECTED_RETURN(quota);
    auto period = str::to_unsigned(fields[1], 10);
    SIHD_UNEXPECTED_RETURN(period);
    return CpuLimit {*quota, static_cast<uint64_t>(*period)};
}

Result<void> write_memory_max(std::optional<uint64_t> bytes)
{
    auto path = group_file("memory.max");
    SIHD_UNEXPECTED_RETURN(path);
    return internal::sysfs_write(*path, max_str(bytes));
}

Result<void> write_cpu_max(std::optional<uint64_t> quota_us, uint64_t period_us)
{
    auto path = group_file("cpu.max");
    SIHD_UNEXPECTED_RETURN(path);
    return internal::sysfs_write(*path, max_str(quota_us) + " " + std::to_string(period_us));
}

} // namespace sihd::sys::linux_::cgroup

#endif
