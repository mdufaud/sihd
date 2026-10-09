#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/LineReader.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/Uuid.hpp>
#include <sihd/sys/clipboard.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/sys/program.hpp>
#include <sihd/sys/screenshot.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/fmt.hpp>
#include <sihd/util/macro.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/time.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# include "emscripten.h"
#endif

using namespace sihd::util;
using namespace sihd::sys;

namespace demo
{

SIHD_NEW_LOGGER("demo");

void os()
{
    SIHD_LOG(info, "pid: {}", sihd::sys::os::pid());
    SIHD_LOG(info, "is_root: {}", sihd::sys::user::is_root());
    SIHD_LOG(info, "max_fds: {}", sihd::sys::os::max_fds());
    SIHD_LOG(info, "debugger: {}", sihd::sys::os::is_run_by_debugger());
    SIHD_LOG(info, "valgrind: {}", sihd::sys::os::is_run_by_valgrind());
    SIHD_LOG(info, "peak_rss: {}", str::bytes_str(sihd::sys::os::peak_rss()));
    SIHD_LOG(info, "current_rss: {}", str::bytes_str(sihd::sys::os::current_rss()));
    fmt::print("\n");
}

void fs()
{
    SIHD_LOG(info, "tmp_path: {}", fs::tmp_path());
    SIHD_LOG(info, "home_path: {}", fs::home_path());
    SIHD_LOG(info, "cwd: {}", fs::cwd());
    SIHD_LOG(info, "program_path: {}", program::path());

    {
        TmpDir tmp;
        SIHD_LOG(info, "TmpDir: {}", tmp.path());

        SIHD_LOG(info, "exists: {}", fs::exists(tmp));
        SIHD_LOG(info, "is_file: {}", fs::is_file(tmp));
        SIHD_LOG(info, "is_dir: {}", fs::is_dir(tmp));
        SIHD_LOG(info, "file_size: {}", fs::file_size(tmp).value_or(0));
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
    SIHD_LOG(info, "uuid assigned: {}", id.str());
    SIHD_LOG(info, "uuid namespace: {}", Uuid(Uuid::DNS(), "sihd").str());
    fmt::print("\n");
}

void dynlib()
{
    DynLib lib;

    if (lib.open("sihd_util"))
    {
        SIHD_LOG(info, "Opened DLL sihd_util");

        auto handle = lib.load_symbol("sihd_factory_Node");
        if (handle.has_value())
            SIHD_LOG(info, "Handle found in DLL");

        lib.close();
    }
    fmt::print("\n");
}

void read_line()
{
    if (term::is_interactive() == false)
        return;
    fmt::print("Say something: ");
    std::cout.flush();
    auto input = LineReader::fast_read_stdin();
    if (input)
        fmt::print("You said: '{}'\n", input.value());
    fmt::print("\n");
}

void file_mem_write()
{
    File file;
    SIHD_DIE_FALSE(file.set_buffer_size(128).has_value());
    SIHD_DIE_FALSE(file.open_mem("r+").has_value());

    const auto write_size = file.write("hello world !");
    (void)file.seek_begin(0);

    ArrChar str(128);
    const auto read_size = file.read(str);

    SIHD_LOG(info,
             "Memory file wrote {} and read {} with content: {}",
             write_size.value_or(0),
             read_size.value_or(0),
             str);

    fmt::print("\n");
}

void file_mem_read()
{
    File file;
    SIHD_DIE_FALSE(file.set_buffer_size(128).has_value());
    SIHD_DIE_FALSE(file.open_mem("r", "hello world !").has_value());

    ArrChar str(128);
    const auto read_size = file.read(str);

    SIHD_LOG(info, "Memory file read {} with content: {}", read_size.value_or(0), str);
    fmt::print("\n");
}

void clipboard()
{
    for (const auto & raw : clipboard::get_raw())
    {
        SIHD_LOG(info, "Clipboard offers '{}' ({} bytes)", raw.mime, raw.data.size());
    }

    for (const auto & raw : clipboard::get_raw({sihd::util::mime::bmp_image}))
    {
        auto bitmap = clipboard::to_image(raw);
        if (!bitmap.has_value())
            continue;

        SIHD_LOG(info,
                 "You have an image in your clipboard: {}x{} ({}bpp)",
                 bitmap->width(),
                 bitmap->height(),
                 bitmap->byte_per_pixel() * 8);
        std::string path = fs::combine(fs::tmp_path(), "clipboard_image.bmp");
        if (bitmap->save_bmp(path))
            SIHD_LOG(notice, "Saved clipboard image to: {}", path);

        // for the fun of it all
        clipboard::set(*bitmap);
        break;
    }

    for (const auto & raw : clipboard::get_raw({sihd::util::mime::utf8_text}))
    {
        auto text = clipboard::to_text(raw);
        if (text.has_value())
            SIHD_LOG(info, "You have text in your clipboard: '{}'", *text);
        break;
    }

    if (clipboard::set("love you"))
    {
        SIHD_LOG(info, "Set 'love you' in your clipboard");
    }
    else
    {
        SIHD_LOG(error, "Could not set clipboard");
    }

    fmt::print("\n");
}

void bitmap()
{
    SIHD_LOG(info, "Testing bitmaps");

    constexpr size_t width = 1024;
    constexpr size_t height = 1024;

    Bitmap bm(width, height);

    Color p;

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

    SIHD_TRACEV(fs::are_equals(first_bmp, second_bmp));
    fmt::print("\n");
}

void screenshot()
{
    Bitmap bitmap;

    SIHD_LOG(info, "Trying to take a screenshot of the window under your cursor");
    if (screenshot::take_under_cursor(bitmap))
    {
        std::string path = fs::combine(fs::tmp_path(), "take_under_cursor.bmp");
        if (bitmap.save_bmp(path))
            SIHD_LOG(notice, "Saved bitmap to: {}", path);
    }

    SIHD_LOG(info, "Trying to take a screenshot of the focused window");
    if (screenshot::take_focused(bitmap))
    {
        std::string path = fs::combine(fs::tmp_path(), "take_focused.bmp");
        if (bitmap.save_bmp(path))
            SIHD_LOG(notice, "Saved bitmap to: {}", path);
    }

    SIHD_LOG(info, "Trying to take a screenshot of the window under your entire screen");
    if (screenshot::take_screen(bitmap))
    {
        std::string path = fs::combine(fs::tmp_path(), "take_screen.bmp");
        if (bitmap.save_bmp(path))
            SIHD_LOG(notice, "Saved bitmap to: {}", path);
    }
    fmt::print("\n");
}

void process()
{
    std::vector<std::string> args;
    proc::Options options({.timeout = std::chrono::milliseconds(100)});
    options.stdout_callback = [](std::string_view stdout_str) {
        fmt::print("=========================\n");
        fmt::print("STDOUT:\n");
        fmt::print("{}\n", stdout_str);
        fmt::print("=========================\n");
    };
    options.stderr_callback = [](std::string_view stderr_str) {
        fmt::print("=========================\n");
        fmt::print("STDERR:\n");
        fmt::print("{}\n", stderr_str);
        fmt::print("=========================\n");
    };

    if constexpr (sihd::util::build::is_unix)
    {
        args.push_back("ls");
        args.push_back("-l");
    }

    if constexpr (sihd::util::build::is_windows)
    {
        term::set_output_utf8();
        args.push_back("cmd.exe");
        args.push_back("/c");
        args.push_back("dir");
    }

    SIHD_LOG(info, "Executing: {}", fmt::join(args, " "));
    auto exit_code = proc::execute(args, options);

    if (exit_code.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready)
    {
        SIHD_LOG(info, "Exit code: {}", exit_code.get());
    }
    else
    {
        SIHD_LOG(error, "Process timeout");
    }
    fmt::print("\n");
}

void backtrace()
{
    sihd::sys::os::backtrace(1, 20);
    fmt::print("\n");
}

} // namespace demo

int main(int argc, char **argv)
{
    App app({
        .name = "sys_demo",
        .description = "Testing utility for module sys",
    });

    app.root().add_command("os", "process informations").on_run([] { demo::os(); });
    app.root().add_command("fs", "filesystem informations").on_run([] { demo::fs(); });
    app.root().add_command("uuid", "uuid generation").on_run([] { demo::uuid(); });
    app.root().add_command("dynlib", "dynamic library loading").on_run([] { demo::dynlib(); });
    app.root().add_command("file", "in memory files").on_run([] {
        demo::file_mem_read();
        demo::file_mem_write();
    });
    app.root().add_command("bitmap", "bitmap save and read").on_run([] { demo::bitmap(); });
    app.root().add_command("backtrace", "print a backtrace").on_run([] { demo::backtrace(); });

#if defined(__SIHD_EMSCRIPTEN__)
    app.root().add_command("readline", "read a line from stdin").on_run([] { demo::read_line(); });
#else
    if constexpr (clipboard::supported)
    {
        app.root().add_command("clipboard", "clipboard content").on_run([] { demo::clipboard(); });
    }
    if constexpr (screenshot::supported)
    {
        app.root().add_command("screenshot", "take screenshots").on_run([] { demo::screenshot(); });
    }
    app.root().add_command("process", "execute a process").on_run([] { demo::process(); });
    app.root().add_command("readline", "read a line from stdin").on_run([] { demo::read_line(); });
    app.root().add_command("wait", "wait 5 seconds or until a stop signal").on_run([&app] {
        fmt::print("Press Ctrl + C to exit (or wait 5 seconds)\n");
        app.wait_for_termination(sihd::util::Duration(time::seconds(5)));
        fmt::print("Exiting...\n");
    });
#endif

    app.root().on_run([&app] {
        demo::os();
        demo::fs();
        demo::uuid();
        demo::dynlib();
        demo::file_mem_read();
        demo::file_mem_write();
        demo::bitmap();
        demo::backtrace();
#if defined(__SIHD_EMSCRIPTEN__)
        demo::read_line();
#else
        if constexpr (clipboard::supported)
            demo::clipboard();
        if constexpr (screenshot::supported)
            demo::screenshot();
        demo::process();
        demo::read_line();
        fmt::print("Press Ctrl + C to exit (or wait 5 seconds)\n");
        app.wait_for_termination(sihd::util::Duration(time::seconds(5)));
        fmt::print("Exiting...\n");
#endif
    });

    return app.run(argc, argv);
}
