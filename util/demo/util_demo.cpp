#include <cxxopts.hpp>
#include <fmt/format.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/Bitmap.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/DynLib.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/Uuid.hpp>
#include <sihd/util/clipboard.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/macro.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/proc.hpp>
#include <sihd/util/screenshot.hpp>
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

void dynlib()
{
    DynLib lib;

    if (lib.open("sihd_util"))
    {
        SIHD_LOG_INFO("Opened DLL sihd_util");

        void *handle = lib.load("sihd_util_namedfactory_Node");
        if (handle != nullptr)
            SIHD_LOG_INFO("Handle found in DLL");

        lib.close();
    }
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

void clipboard()
{
    auto opt_str = clipboard::get();
    if (opt_str)
    {
        SIHD_LOG_INFO("You have '{}' in you clipboard", *opt_str);
    }
    else
    {
        SIHD_LOG_ERROR("Could not get clipboard data");
    }

    if (clipboard::set("love you"))
    {
        SIHD_LOG_INFO("Setted 'love you' in your clipboard");
    }
    else
    {
        SIHD_LOG_ERROR("Could not set clipboard");
    }

    fmt::print("\n");
}

void bitmap()
{
    SIHD_LOG_INFO("Testing bitmaps");

    constexpr size_t width = 1024;
    constexpr size_t height = 1024;

    Bitmap bm(width, height);

    Pixel p;

    p.alpha = 0;
    p.red = 200;
    p.blue = 0;
    p.green = 0;

    bm.fill(p);

    TmpDir tmp;

    std::string first_bmp = fs::combine(tmp.path(), "toto.bmp");
    std::string second_bmp = fs::combine(tmp.path(), "toto2.bmp");

    bm.save_bmp(first_bmp);

    Bitmap bm2;
    bm2.read_bmp(first_bmp);
    bm2.save_bmp(second_bmp);

    SIHD_TRACEF(fs::are_equals(first_bmp, second_bmp));
    fmt::print("\n");
}

void screenshot()
{
    Bitmap bitmap;

    SIHD_LOG_INFO("Trying to take a screenshot of the window under your cursor");
    if (screenshot::take_under_cursor(bitmap))
    {
        std::string path = fs::combine(fs::tmp_path(), "take_under_cursor.bmp");
        if (bitmap.save_bmp(path))
            SIHD_LOG(notice, "Saved bitmap to: {}", path);
    }

    SIHD_LOG_INFO("Trying to take a screenshot of the focused window");
    if (screenshot::take_focused(bitmap))
    {
        std::string path = fs::combine(fs::tmp_path(), "take_focused.bmp");
        if (bitmap.save_bmp(path))
            SIHD_LOG(notice, "Saved bitmap to: {}", path);
    }

    SIHD_LOG_INFO("Trying to take a screenshot of the window under your entire screen");
    if (screenshot::take_screen(bitmap))
    {
        std::string path = fs::combine(fs::tmp_path(), "take_screen.bmp");
        if (bitmap.save_bmp(path))
            SIHD_LOG(notice, "Saved bitmap to: {}", path);
    }
}

void process()
{
    std::vector<std::string> args;
    proc::Options options({.timeout = std::chrono::milliseconds(100)});
    options.stdout_callback = [](std::string_view stdout_str) {
        fmt::print("=========================\n");
        fmt::print("STDOUT: {}\n", stdout_str);
        fmt::print("=========================\n");
    };
    options.stderr_callback = [](std::string_view stderr_str) {
        fmt::print("=========================\n");
        fmt::print("STDERR: {}\n", stderr_str);
        fmt::print("=========================\n");
    };

    if constexpr (os::is_unix)
    {
        args.push_back("ls");
    }

    if constexpr (os::is_windows)
    {
        args.push_back("cmd.exe");
        args.push_back("/c");
        args.push_back("dir");
    }

    SIHD_LOG(info, "Executing: {}", fmt::join(args, " "));
    auto exit_code = proc::execute(args, options);

    if (exit_code.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready)
    {
        SIHD_LOG(info, "Exit code: {}", exit_code.get());
    }
    else
    {
        SIHD_LOG(error, "Process timeout");
    }
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
        fmt::print("{}\n", options.help());
        return 0;
    }

    if (term::is_interactive() && os::is_unix && !os::is_emscripten)
        LoggerManager::console();
    else
        LoggerManager::stream(os::is_emscripten ? stdout : stderr);

    demo::time();
    demo::os();
    demo::fs();
    demo::uuid();
    demo::dynlib();
    demo::file_mem_read();
    demo::file_mem_write();
    demo::clipboard();
    demo::bitmap();
    demo::screenshot();
    demo::process();

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
