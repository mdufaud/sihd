#include <cxxopts.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/Uuid.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/macro.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# include "emscripten.h"
#endif

using namespace sihd::util;

namespace demo
{

SIHD_NEW_LOGGER("demo");

void worker(double frequency)
{
    Runnable printer([] {
        SIHD_LOG(info, "time since epoch: {}", Timestamp::now().timeoffset_str());
        return true;
    });

    StepWorker worker;

    worker.set_runnable(&printer);
    worker.set_frequency(frequency);
    worker.start_sync_worker("worker");

    Waitable waiter;
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
    SIHD_LOG(info, "today's day: {}", today.local_day_str());
    SIHD_LOG(info, "today's time: {}", today.local_sec_str());
    SIHD_LOG(info, "today's hour: {}", today.local_clocktime().hour);
    SIHD_LOG(info, "today's zoned hour: {}", today.zone_str());
    SIHD_LOG(info, "from string date '2000/04/01': {}", Timestamp::from_str("2000/04/01", "%Y/%m/%d")->str());
    SIHD_LOG(info, "timezone name: {}", time::get_timezone_name());
    SIHD_LOG(info, "timezone offset: {}", time::get_timezone());

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
        SIHD_LOG(info, "file_size: {}", fs::file_size(tmp));
        SIHD_LOG(info, "last_write: {}", fs::last_write(tmp).local_str());
        SIHD_LOG(info, "is_readable: {}", fs::is_readable(tmp));
        SIHD_LOG(info, "is_writable: {}", fs::is_writable(tmp));
        SIHD_LOG(info, "is_executable: {}", fs::is_executable(tmp));
    }

    fmt::print("\n");
}

void uuid()
{
    Uuid id;
    SIHD_LOG(info, "uuid: {}", id.str());
    id = Uuid();
    SIHD_LOG(info, "uuid2: {}", id.str());
    fmt::print("\n");
}

void read_line()
{
    if (term::is_interactive() == false)
        return;
    fmt::print("Say something: ");
    std::string input;
    LineReader::fast_read_stdin(input);
    fmt::print("You said: '{}'\n", input);
    fmt::print("\n");
}

void file_mem_write()
{
    File file;
    file.set_buffer_size(128);
    SIHD_DIE_FALSE(file.open_mem("r+"));

    ssize_t write_size = file.write("hello world !");
    file.seek_begin(0);

    ArrChar str(128);
    ssize_t read_size = file.read(str);

    SIHD_LOG(info, "Memory file wrote {} and read {} with content: {}", write_size, read_size, str);
    fmt::print("\n");
}

void file_mem_read()
{
    File file;
    file.set_buffer_size(128);
    SIHD_DIE_FALSE(file.open_mem("r", "hello world !"));

    ArrChar str(128);
    ssize_t read_size = file.read(str);

    SIHD_LOG(info, "Memory file read {} with content: {}", read_size, str);
    fmt::print("\n");
}

} // namespace demo

int main(int argc, char **argv)
{
    cxxopts::Options options(argv[0], "Testing utility for module util");
    // clang-format off
    options.add_options()
        ("h,help", "Prints usage")
        ("f,worker-frequency", "Change the worker execution frequency in HZ", cxxopts::value<double>()->default_value("10.0"));
    // clang-format on

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        fmt::print(options.help());
        return 0;
    }

    if (term::is_interactive() && os::is_unix && !os::is_emscripten)
        LoggerManager::console();
    else
        LoggerManager::basic(os::is_emscripten ? stdout : stderr);

    demo::time();
    demo::os();
    demo::fs();
    demo::uuid();
    demo::file_mem_read();
    demo::file_mem_write();

    demo::worker(result["worker-frequency"].as<double>());

    if constexpr (os::is_emscripten == false)
    {
        demo::read_line();

        fmt::print("Press Ctrl + c to exit\n");
        SigWaiter waiter;
        fmt::print("Exiting...\n");
    }

    return 0;
}
