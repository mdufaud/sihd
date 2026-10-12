#include <set>

#include <sihd/sys/info.hpp>
#include <sihd/util/Clocks.hpp>

#include "internal/process_monitor.hpp"

namespace sihd::sys
{

using namespace sihd::util;

namespace
{

uint64_t process_cpu_ns(const ProcessMetrics & metrics)
{
    return static_cast<uint64_t>(metrics.user_time.nanoseconds())
           + static_cast<uint64_t>(metrics.system_time.nanoseconds());
}

} // namespace

ProcessMonitor::ProcessMonitor()
{
    _impl = std::make_unique<Impl>();
    _impl->worker.set_runnable(this);
}

ProcessMonitor::~ProcessMonitor()
{
    _impl->worker.stop_worker();
}

bool ProcessMonitor::set_frequency(double hz)
{
    return _impl->worker.set_frequency(hz);
}

bool ProcessMonitor::set_name_filter(const std::string & regex)
{
    std::lock_guard lock(_impl->mutex);
    _impl->name_filter = regex;
    return true;
}

bool ProcessMonitor::start()
{
    return _impl->worker.start_sync_worker("process_monitor");
}

bool ProcessMonitor::stop()
{
    return _impl->worker.stop_worker();
}

bool ProcessMonitor::is_running() const
{
    return _impl->worker.is_worker_running();
}

bool ProcessMonitor::poll()
{
    std::string name_filter;
    {
        std::lock_guard lock(_impl->mutex);
        name_filter = _impl->name_filter;
    }
    std::vector<ProcessSample> fresh = _impl->collect(name_filter);
    _impl->finish_pass(std::move(fresh));
    this->notify_observers(this);
    std::lock_guard lock(_impl->mutex);
    return _impl->samples.empty() == false;
}

bool ProcessMonitor::run()
{
    // an empty pass must not stop the worker
    (void)this->poll();
    return true;
}

std::vector<ProcessSample> ProcessMonitor::samples() const
{
    std::lock_guard lock(_impl->mutex);
    return _impl->samples;
}

std::optional<ProcessSample> ProcessMonitor::sample(int pid) const
{
    std::lock_guard lock(_impl->mutex);
    for (const ProcessSample & sample : _impl->samples)
    {
        if (sample.metrics.pid == pid)
            return sample;
    }
    return std::nullopt;
}

void ProcessMonitor::Impl::finish_pass(std::vector<ProcessSample> fresh)
{
    auto system_times = info::cpu_times();
    const uint64_t busy_now = system_times.has_value() ? system_times->busy() : 0;
    const Timestamp now = SystemClock().now();

    std::lock_guard lock(mutex);
    for (ProcessSample & sample : fresh)
    {
        sample.timestamp = now;
        const uint64_t cpu_now = process_cpu_ns(sample.metrics);
        auto prev = cpu_history.find(sample.metrics.pid);
        if (prev != cpu_history.end() && busy_now > prev->second.second)
        {
            // a reused pid can rewind its cpu time: the delta clamps to zero
            const uint64_t cpu_delta = cpu_now > prev->second.first ? cpu_now - prev->second.first : 0;
            sample.cpu_percent = 100.0 * static_cast<double>(cpu_delta)
                                 / static_cast<double>(busy_now - prev->second.second);
        }
        cpu_history[sample.metrics.pid] = {cpu_now, busy_now};
    }
    std::set<int> alive;
    for (const ProcessSample & sample : fresh)
        alive.insert(sample.metrics.pid);
    std::erase_if(cpu_history, [&alive](const auto & entry) { return alive.contains(entry.first) == false; });
    samples = std::move(fresh);
}

} // namespace sihd::sys
