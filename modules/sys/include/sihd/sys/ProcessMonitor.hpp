#ifndef __SIHD_SYS_PROCESSMONITOR_HPP__
#define __SIHD_SYS_PROCESSMONITOR_HPP__

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

struct ProcessSample
{
        std::string name;
        ProcessMetrics metrics;
        // 100 = all cores busy, computed since the previous pass
        double cpu_percent = 0.0;
        sihd::util::Timestamp timestamp;
};

class ProcessMonitor: public sihd::util::Observable<ProcessMonitor>,
                      public sihd::util::IRunnable
{
    public:
        static constexpr bool supported = ProcessInfo::supported;

        ProcessMonitor();
        ~ProcessMonitor();

        bool set_frequency(double hz);
        bool set_name_filter(const std::string & regex);

        bool start();
        bool stop();
        bool is_running() const;
        bool poll();

        // copies: the worker thread refreshes the samples between two calls
        std::vector<ProcessSample> samples() const;
        std::optional<ProcessSample> sample(int pid) const;

        bool run() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
