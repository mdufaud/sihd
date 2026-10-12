#include <unistd.h>

#include <sihd/util/str.hpp>

#include "../internal/process_monitor.hpp"
#include "internal/proc_stat.hpp"

namespace sihd::sys
{

using namespace sihd::util;

std::vector<ProcessSample> ProcessMonitor::Impl::collect(const std::string & name_filter)
{
    std::vector<ProcessSample> fresh;
    for (int pid : ProcessInfo::pids())
    {
        std::string name;
        auto metrics = internal::read_proc_metrics(pid, &name);
        if (metrics.has_value() == false)
            continue;
        if (name_filter.empty() == false && str::regex_match(name, name_filter) == false)
            continue;
        ProcessSample sample;
        sample.name = std::move(name);
        sample.metrics = std::move(*metrics);
        fresh.push_back(std::move(sample));
    }
    return fresh;
}

} // namespace sihd::sys
