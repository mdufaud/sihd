#ifndef __SIHD_SYS_SCHED_HPP__
#define __SIHD_SYS_SCHED_HPP__

#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

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

sihd::util::Result<int> nice(pid_t pid);
sihd::util::Result<void> set_nice(pid_t pid, int nice_value);
sihd::util::Result<Policy> scheduler(pid_t pid);
sihd::util::Result<Priority> scheduler_priority(pid_t pid);
sihd::util::Result<void> set_scheduler(pid_t pid, Policy policy, Priority priority);
// index is the cpu id; limited to 64 cpus on windows (single processor group)
sihd::util::Result<std::vector<bool>> affinity(pid_t pid);
sihd::util::Result<void> set_affinity(pid_t pid, const std::vector<bool> & cpu_mask);

} // namespace sihd::sys::sched

#endif
