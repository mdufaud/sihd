#include <sched.h>
#include <sys/resource.h>

#include <sihd/sys/sched.hpp>
#include <sihd/util/Result.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::sched
{

namespace
{

Policy scheduler_from_policy(int policy)
{
    switch (policy)
    {
        case SCHED_BATCH:
            return Policy::batch;
        case SCHED_IDLE:
            return Policy::idle;
        case SCHED_FIFO:
            return Policy::fifo;
        case SCHED_RR:
            return Policy::rr;
        case SCHED_DEADLINE:
            return Policy::deadline;
        case SCHED_OTHER:
        default:
            return Policy::other;
    }
}

int policy_from_scheduler(Policy policy)
{
    switch (policy)
    {
        case Policy::batch:
            return SCHED_BATCH;
        case Policy::idle:
            return SCHED_IDLE;
        case Policy::fifo:
            return SCHED_FIFO;
        case Policy::rr:
            return SCHED_RR;
        case Policy::deadline:
            return SCHED_DEADLINE;
        case Policy::other:
            return SCHED_OTHER;
    }
    return SCHED_OTHER;
}

} // namespace

sihd::util::Result<int> nice(pid_t pid)
{
    errno = 0;
    const int value = getpriority(PRIO_PROCESS, pid);
    // a returned -1 is only a failure when errno says so: nice values go that low
    if (value == -1 && errno != 0)
        return std::unexpected(Error::from_errno("getpriority({})", pid));
    return value;
}

sihd::util::Result<void> set_nice(pid_t pid, int nice_value)
{
    if (setpriority(PRIO_PROCESS, pid, nice_value) == -1)
        return std::unexpected(Error::from_errno("setpriority({}, {})", pid, nice_value));
    return {};
}

sihd::util::Result<Policy> scheduler(pid_t pid)
{
    const int policy = sched_getscheduler(pid);
    if (policy == -1)
        return std::unexpected(Error::from_errno("sched_getscheduler({})", pid));
    return scheduler_from_policy(policy);
}

sihd::util::Result<Priority> scheduler_priority(pid_t pid)
{
    sched_param param = {};
    if (sched_getparam(pid, &param) == -1)
        return std::unexpected(Error::from_errno("sched_getparam({})", pid));
    return Priority {param.sched_priority};
}

sihd::util::Result<void> set_scheduler(pid_t pid, Policy policy, Priority priority)
{
    // SCHED_DEADLINE is only settable through sched_setattr with the runtime/deadline/period triple
    if (policy == Policy::deadline)
        return std::unexpected(Error(invalid_argument, "SCHED_DEADLINE requires sched_setattr"));
    sched_param param = {};
    param.sched_priority = priority.value;
    if (sched_setscheduler(pid, policy_from_scheduler(policy), &param) == -1)
        return std::unexpected(Error::from_errno("sched_setscheduler({}, {})", pid, static_cast<int>(policy)));
    return {};
}

sihd::util::Result<std::vector<bool>> affinity(pid_t pid)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(pid, sizeof(set), &set) == -1)
        return std::unexpected(Error::from_errno("sched_getaffinity({})", pid));
    size_t size = 0;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
    {
        if (CPU_ISSET(cpu, &set))
            size = static_cast<size_t>(cpu) + 1;
    }
    std::vector<bool> cpu_mask(size, false);
    for (size_t cpu = 0; cpu < size; ++cpu)
        cpu_mask[cpu] = CPU_ISSET(static_cast<int>(cpu), &set);
    return cpu_mask;
}

sihd::util::Result<void> set_affinity(pid_t pid, const std::vector<bool> & cpu_mask)
{
    if (cpu_mask.empty())
        return std::unexpected(Error(invalid_argument, "empty cpu mask"));
    cpu_set_t set;
    CPU_ZERO(&set);
    for (size_t cpu = 0; cpu < cpu_mask.size(); ++cpu)
    {
        if (static_cast<size_t>(CPU_SETSIZE) <= cpu)
            return std::unexpected(Error(invalid_argument, "cpu {} out of range", cpu));
        if (cpu_mask[cpu])
            CPU_SET(static_cast<int>(cpu), &set);
    }
    if (sched_setaffinity(pid, sizeof(set), &set) == -1)
        return std::unexpected(Error::from_errno("sched_setaffinity({})", pid));
    return {};
}

} // namespace sihd::sys::sched
