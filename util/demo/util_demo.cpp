#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/LoadingBar.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;

namespace demo
{

SIHD_NEW_LOGGER("demo");

void worker(double frequency)
{
    constexpr std::chrono::seconds work_time(2);

    LoadingBar bar({
        .width = 30,
        .total = (size_t)frequency * (size_t)work_time.count(),
        .percentage = true,
    });

    Runnable printer([&bar] {
        bar.add_progress(1);
        bar.print("Working ", " ...");
        return true;
    });

    StepWorker worker;
    worker.set_runnable(&printer);
    worker.set_frequency(frequency);

    fmt::print("Starting worker\n");

    worker.start_sync_worker("worker");
    std::this_thread::sleep_for(work_time);
    worker.stop_worker();

    fmt::print("\nEnded worker\n");
    fmt::print("\n");
}

void time()
{
    Timestamp today = Timestamp::now();
    SIHD_LOG(info, "today's day: {}", today.local_day_str());
    SIHD_LOG(info, "today's time: {}", today.local_sec_str());
    SIHD_LOG(info, "today's hour: {}", today.local_clocktime().hour);
    if constexpr (!sihd::util::platform::is_windows)
    {
        // no %z in windows
        SIHD_LOG(info, "today's zoned hour: {}", today.zone_str());
    }
    SIHD_LOG(info, "from string date '2000/04/01': {}", Timestamp::from_str("2000/04/01", "%Y/%m/%d")->str());

    // Locale examples - uses C locale by default for deterministic behavior
    SIHD_LOG(info, "format with C locale (default): {}", today.format("%Y-%m-%d %H:%M:%S"));
    SIHD_LOG(info,
             "format with explicit C locale: {}",
             today.format("%Y-%m-%d %H:%M:%S", std::locale::classic()));

    SIHD_LOG(info, "timezone name: {}", time::get_timezone_name());
    SIHD_LOG(info, "timezone offset: {}", time::get_timezone());
    SIHD_LOG(info, "time since epoch: {}", Timestamp::now().timeoffset_str());
    fmt::print("\n");
}

} // namespace demo

int main(int argc, char **argv)
{
    double worker_frequency = 10.0;
    CLI::App app {"Testing utility for module util"};
    app.add_option("-f,--worker-frequency", worker_frequency, "Change the worker execution frequency in HZ")
        ->default_val("10.0");

    CLI11_PARSE(app, argc, argv);

    if (term::is_interactive() && !sihd::util::platform::is_emscripten)
        LoggerManager::console();
    else
        LoggerManager::stream(sihd::util::platform::is_emscripten ? stdout : stderr);

    demo::time();
    demo::worker(worker_frequency);

    return 0;
}
