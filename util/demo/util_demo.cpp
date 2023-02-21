#include <cxxopts.hpp>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/Uuid.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

using namespace sihd::util;

namespace demo
{

SIHD_NEW_LOGGER("demo");

void worker(double frequency)
{
    Waitable waiter;

    Runnable printer([] {
        SIHD_LOG(info, "Time since epoch: {}", Timestamp::now().timeoffset_str());
        return true;
    });

    StepWorker worker;
    worker.set_runnable(&printer);
    worker.set_frequency(frequency);
    worker.start_sync_worker("worker");

    waiter.wait_for(std::chrono::seconds(1));

    worker.stop_worker();
    fmt::print("\n");
}

void os()
{
    SIHD_LOG(info, "pid: {}", os::pid());
    SIHD_LOG(info, "is_root: {}", os::is_root());
    SIHD_LOG(info, "max_fds: {}", os::max_fds());
    SIHD_LOG(info, "debugger: {}", os::is_run_by_debugger());
    SIHD_LOG(info, "valgrind: {}", os::is_run_by_valgrind());
    SIHD_LOG(info, "peak_rss: {}", str::bytes_str(os::peak_rss()));
    SIHD_LOG(info, "current_rss: {}", str::bytes_str(os::current_rss()));
    fmt::print("\n");
}

void time()
{
    Timestamp today = Timestamp::now();
    SIHD_LOG(info, "Today's day: {}", today.local_day_str());
    SIHD_LOG(info, "Today's time: {}", today.local_sec_str());
    SIHD_LOG(info, "Today's hour: {}", today.local_clocktime().hour);
    SIHD_LOG(info, "Today's zoned hour: {}", today.zone_str());
    SIHD_LOG(info, "From string date '2000/04/01': {}", Timestamp::from_str("2000/04/01", "%Y/%m/%d")->str());
    SIHD_LOG(info, "Timezone name: {}", time::get_timezone_name());
    SIHD_LOG(info, "Timezone offset: {}", time::get_timezone());
    fmt::print("\n");
}

void fs()
{
    SIHD_LOG(info, "tmp_path: {}", fs::tmp_path());
    SIHD_LOG(info, "home_path: {}", fs::home_path());
    SIHD_LOG(info, "cwd: {}", fs::cwd());
    SIHD_LOG(info, "executable_path: {}", fs::executable_path());

    {
        TmpDir tmp;
        SIHD_LOG(info, "TmpDir: {}", tmp.path());

        SIHD_LOG(info, "exists: {}", fs::exists(tmp));
        SIHD_LOG(info, "is_file: {}", fs::is_file(tmp));
        SIHD_LOG(info, "is_dir: {}", fs::is_dir(tmp));
        SIHD_LOG(info, "filesize: {}", fs::filesize(tmp));
        SIHD_LOG(info, "last_write: {}", fs::last_write(tmp).str());
        SIHD_LOG(info, "is_readable: {}", fs::is_readable(tmp));
        SIHD_LOG(info, "is_writable: {}", fs::is_writable(tmp));
        SIHD_LOG(info, "is_executable: {}", fs::is_executable(tmp));
    }

    fmt::print("\n");
}

void uuid()
{
    Uuid id;
    SIHD_LOG(info, "Uuid: {}", id.str());
    id = Uuid();
    SIHD_LOG(info, "Uuid2: {}", id.str());
    fmt::print("\n");
}

void read_line()
{
    if (term::is_interactive() == false)
        return;
    fmt::print("Say something: ");
    std::string input;
    LineReader::fast_read_line(input);
    fmt::print("You said: '{}'\n", input);
    fmt::print("\n");
}

} // namespace demo

int main(int argc, char **argv)
{
    if (term::is_interactive() && os::is_unix)
        LoggerManager::console();
    else
        LoggerManager::basic();

    Defer d([] { LoggerManager::clear_loggers(); });

    cxxopts::Options options(argv[0], "Testing utility for module util");
    options.add_options()("h,help", "Prints usage")("f,worker-frequency",
                                                    "Change the worker execution frequency in HZ",
                                                    cxxopts::value<double>()->default_value("10.0"));

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        fmt::print(options.help());
        return 0;
    }

    demo::time();
    demo::os();
    demo::fs();
    demo::uuid();
    demo::worker(result["worker-frequency"].as<double>());
    demo::read_line();

    fmt::print("Press Ctrl + c to exit\n");
    SigWaiter waiter;

    return 0;
}
