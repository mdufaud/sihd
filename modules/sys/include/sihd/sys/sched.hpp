#ifndef __SIHD_SYS_SCHED_HPP__
#define __SIHD_SYS_SCHED_HPP__

#include <expected>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::sys::sched
{

// scheduling policies; windows has no equivalent and reports not_supported
enum class Policy
{
    other,
    batch,
    idle,
    fifo,
    rr,
    deadline,
};

// realtime policies run 1..99, the others run 0; windows anchors its priority classes
struct Priority
{
        int value = 0;
};

std::expected<int, sihd::util::Error> nice(pid_t pid);
std::expected<void, sihd::util::Error> set_nice(pid_t pid, int nice_value);
std::expected<Policy, sihd::util::Error> scheduler(pid_t pid);
std::expected<Priority, sihd::util::Error> scheduler_priority(pid_t pid);
std::expected<void, sihd::util::Error> set_scheduler(pid_t pid, Policy policy, Priority priority);
// index is the cpu id; limited to 64 cpus on windows (single processor group)
std::expected<std::vector<bool>, sihd::util::Error> affinity(pid_t pid);
std::expected<void, sihd::util::Error> set_affinity(pid_t pid, const std::vector<bool> & cpu_mask);

} // namespace sihd::sys::sched

#endif
