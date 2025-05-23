#include <cxxopts.hpp>
#include <fmt/format.h>

#include <sihd/util/FileWatcher.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/signal.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

using namespace sihd::util;

SIHD_NEW_LOGGER("demo");

int main(int argc, char **argv)
{
    if constexpr (os::is_windows)
    {
        term::set_output_utf8();
    }

    cxxopts::Options options(argv[0], "Testing file watcher");

    // clang-format off
    options.add_options()
        ("h,help", "Prints usage")
        ("p,path", "Watch path for changes", cxxopts::value<std::string>());
    // clang-format on

    options.parse_positional({"path"});

    cxxopts::ParseResult result = options.parse(argc, argv);

    if (result.count("help"))
    {
        fmt::print("{}\n", options.help());
        return 0;
    }

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
                SIHD_LOG(warning, "Event: watch is terminated ({} has probably been deleted)", event.watch_path);
                stop = true;
            }
            else
            {
                SIHD_LOG(info, "Event: {}/{} {}", event.watch_path, event.filename, event.type_str());
            }
        }
    });

    const std::string & watch_path = result["path"].as<std::string>();
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