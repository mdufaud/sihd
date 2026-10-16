#ifndef __SIHD_SYS_INTERNAL_PROCESS_MONITOR_HPP__
#define __SIHD_SYS_INTERNAL_PROCESS_MONITOR_HPP__

#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <sihd/sys/ProcessMonitor.hpp>
#include <sihd/util/StepWorker.hpp>

namespace sihd::sys
{

struct ProcessMonitor::Impl
{
        std::vector<ProcessSample> collect(const std::string & name_filter);
        void finish_pass(std::vector<ProcessSample> fresh, sihd::util::Timestamp wall_now);

        sihd::util::StepWorker worker;
        std::string name_filter;
        mutable std::mutex mutex;
        std::vector<ProcessSample> samples;
        std::map<int, std::pair<uint64_t, sihd::util::Timestamp>> cpu_history;
};

} // namespace sihd::sys

#endif
