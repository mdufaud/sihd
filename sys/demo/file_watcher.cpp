#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/sys/SigHandler.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

using namespace sihd::util;
using namespace sihd::sys;

SIHD_NEW_LOGGER("demo");

int main(int argc, char **argv)
{
    if constexpr (sihd::util::platform::is_windows)
    {
        term::set_output_utf8();
    }

    std::string path;
    CLI::App app {"Testing file watcher"};
    app.add_option("-p,--path", path, "Watch path for changes")->required();
    app.add_option("path", path);

    CLI11_PARSE(app, argc, argv);

    LoggerManager::console();

    bool stop = false;
    Handler<FileWatcher *> handler([&stop](FileWatcher *fw) {
        for (const FileWatcherEvent & event : fw->events())
        {
            if (event.type == FileWatcherEventType::renamed)
            {
                SIHD_LOG(notice,
                         "Event: renamed {}/{} -> {}/{}",
                         event.watch_path,
                         event.old_filename,
                         event.watch_path,
                         event.filename);
            }
            else if (event.type == FileWatcherEventType::terminated)
            {
                SIHD_LOG(warning,
                         "Event: watch is terminated ({} has probably been deleted)",
                         event.watch_path);
                stop = true;
            }
            else
            {
                SIHD_LOG(info, "Event: {}/{} {}", event.watch_path, event.filename, event.type_str());
            }
        }
    });

    const std::string & watch_path = path;
    FileWatcher fw;
    if (!fw.watch(watch_path))
    {
        SIHD_LOG(error, "Failed to watch: {}", watch_path);
        return 1;
    }
    constexpr int timeout_milliseconds = 500;
    fw.set_run_timeout(timeout_milliseconds);
    fw.add_observer(&handler);

    SigHandler sig_handler(SIGINT);
    while (!stop && !signal::termination_received())
    {
        fw.run();
    }

    return 0;
}