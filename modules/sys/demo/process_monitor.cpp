#include <algorithm>
#include <chrono>

#include <fmt/format.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/ProcessMonitor.hpp>
#include <sihd/sys/console.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::sys;

SIHD_NEW_LOGGER("demo");

int main(int argc, char **argv)
{
    if constexpr (build::is_windows)
    {
        console::set_output_utf8();
    }

    App app({
        .name = "process_monitor",
        .description = "Sample processes like a mini top",
    });

    double frequency = 1.0;
    double duration = 5.0;
    std::string name_filter;
    app.root().bind("frequency", frequency, "Sampling frequency (Hz)", "f");
    app.root().bind("duration", duration, "Sampling duration (seconds)", "d");
    app.root().bind("filter", name_filter, "Process name regex filter", "r");

    ProcessMonitor monitor;

    Handler<ProcessMonitor *> handler([](ProcessMonitor *m) {
        std::vector<ProcessSample> samples = m->samples();
        std::sort(samples.begin(), samples.end(), [](const ProcessSample & a, const ProcessSample & b) {
            return a.cpu_percent > b.cpu_percent;
        });
        SIHD_LOG(info, "--- {} processes ---", samples.size());
        const size_t count = std::min(samples.size(), size_t(15));
        for (size_t i = 0; i < count; ++i)
        {
            const ProcessSample & sample = samples[i];
            SIHD_LOG(info,
                     "{:>7} {:<20} {:>7.2f}% rss:{:>9} virt:{:>9} threads:{:>3}",
                     sample.metrics.pid,
                     sample.name,
                     sample.cpu_percent,
                     sample.metrics.rss_bytes,
                     sample.metrics.virtual_bytes,
                     sample.metrics.thread_count);
        }
    });
    monitor.add_observer(&handler);
    (void)monitor.set_frequency(frequency);
    if (name_filter.empty() == false)
        (void)monitor.set_name_filter(name_filter);

    app.root().on_run([&] {
        if (monitor.start() == false)
        {
            SIHD_LOG(error, "could not start the process monitor");
            return;
        }
        time::msleep(static_cast<int64_t>(duration * 1000.0));
        (void)monitor.stop();
    });

    return app.run(argc, argv);
}
