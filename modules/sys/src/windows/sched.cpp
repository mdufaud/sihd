#include <processthreadsapi.h>
#include <windows.h>

#include <sihd/sys/os.hpp>
#include <sihd/sys/sched.hpp>
#include <sihd/util/Result.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::sched
{

namespace
{

// single processor group: the masks Get/SetProcessAffinityMask exchange are one ULONG_PTR wide
constexpr size_t max_cpus = sizeof(ULONG_PTR) * 8;

ULONG_PTR cpu_bit(size_t cpu)
{
    return static_cast<ULONG_PTR>(1) << cpu;
}

// nice is approximated by priority classes: coarser granularity, values are class anchors
int nice_from_class(DWORD priority_class)
{
    switch (priority_class)
    {
        case REALTIME_PRIORITY_CLASS:
            return -20;
        case HIGH_PRIORITY_CLASS:
            return -10;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            return -5;
        case BELOW_NORMAL_PRIORITY_CLASS:
            return 10;
        case IDLE_PRIORITY_CLASS:
            return 19;
        case NORMAL_PRIORITY_CLASS:
        default:
            return 0;
    }
}

DWORD class_from_nice(int nice_value)
{
    if (nice_value < -15)
        return REALTIME_PRIORITY_CLASS;
    if (nice_value < -5)
        return HIGH_PRIORITY_CLASS;
    if (nice_value < 0)
        return ABOVE_NORMAL_PRIORITY_CLASS;
    if (nice_value <= 9)
        return NORMAL_PRIORITY_CLASS;
    if (nice_value <= 18)
        return BELOW_NORMAL_PRIORITY_CLASS;
    return IDLE_PRIORITY_CLASS;
}

Result<HANDLE> open_process(pid_t pid, DWORD access)
{
    HANDLE handle = OpenProcess(access, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr)
        return std::unexpected(Error(io_error, "OpenProcess({}): {}", pid, os::last_error_str()));
    return handle;
}

} // namespace

Result<int> nice(pid_t pid)
{
    auto handle = open_process(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    SIHD_UNEXPECTED_RETURN(handle);
    const DWORD priority_class = GetPriorityClass(*handle);
    CloseHandle(*handle);
    if (priority_class == 0)
        return std::unexpected(Error(io_error, "GetPriorityClass({}): {}", pid, os::last_error_str()));
    return nice_from_class(priority_class);
}

Result<void> set_nice(pid_t pid, int nice_value)
{
    auto handle = open_process(pid, PROCESS_SET_INFORMATION);
    SIHD_UNEXPECTED_RETURN(handle);
    const BOOL ret = SetPriorityClass(*handle, class_from_nice(nice_value));
    CloseHandle(*handle);
    if (ret == FALSE)
        return std::unexpected(Error(io_error, "SetPriorityClass({}): {}", pid, os::last_error_str()));
    return {};
}

Result<Policy> scheduler([[maybe_unused]] pid_t pid)
{
    return std::unexpected(Error(not_supported, "scheduler policies are not available on windows"));
}

Result<Priority> scheduler_priority([[maybe_unused]] pid_t pid)
{
    return std::unexpected(Error(not_supported, "scheduler policies are not available on windows"));
}

Result<void>
    set_scheduler([[maybe_unused]] pid_t pid, [[maybe_unused]] Policy policy, [[maybe_unused]] Priority priority)
{
    return std::unexpected(Error(not_supported, "scheduler policies are not available on windows"));
}

Result<std::vector<bool>> affinity(pid_t pid)
{
    auto handle = open_process(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    SIHD_UNEXPECTED_RETURN(handle);
    ULONG_PTR process_mask = 0;
    ULONG_PTR system_mask = 0;
    const BOOL ret = GetProcessAffinityMask(*handle, &process_mask, &system_mask);
    CloseHandle(*handle);
    if (ret == FALSE)
        return std::unexpected(Error(io_error, "GetProcessAffinityMask({}): {}", pid, os::last_error_str()));
    size_t size = 0;
    for (size_t cpu = 0; cpu < max_cpus; ++cpu)
    {
        if (system_mask & cpu_bit(cpu))
            size = cpu + 1;
    }
    std::vector<bool> cpu_mask(size, false);
    for (size_t cpu = 0; cpu < size; ++cpu)
        cpu_mask[cpu] = process_mask & cpu_bit(cpu);
    return cpu_mask;
}

Result<void> set_affinity(pid_t pid, const std::vector<bool> & cpu_mask)
{
    if (cpu_mask.size() > max_cpus)
        return std::unexpected(Error(invalid_argument, "cpu mask limited to 64 cpus on windows"));
    ULONG_PTR mask = 0;
    for (size_t cpu = 0; cpu < cpu_mask.size(); ++cpu)
    {
        if (cpu_mask[cpu])
            mask |= cpu_bit(cpu);
    }
    if (mask == 0)
        return std::unexpected(Error(invalid_argument, "empty cpu mask"));
    auto handle = open_process(pid, PROCESS_SET_INFORMATION);
    SIHD_UNEXPECTED_RETURN(handle);
    const BOOL ret = SetProcessAffinityMask(*handle, mask);
    CloseHandle(*handle);
    if (ret == FALSE)
        return std::unexpected(Error(io_error, "SetProcessAffinityMask({}): {}", pid, os::last_error_str()));
    return {};
}

} // namespace sihd::sys::sched
