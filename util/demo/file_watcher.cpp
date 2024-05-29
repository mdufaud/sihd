#include <cxxopts.hpp>
#include <fmt/format.h>

#include <sihd/util/BasicLogger.hpp>
#include <sihd/util/FileWatcher.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;

std::string display_fw(std::string_view category, const std::vector<std::string> & list)
{
    constexpr size_t max_width = 200;
    return str::wrap(fmt::format("{} [{}]: {}", category, list.size(), fmt::join(list, ",")), max_width);
}

int main(int argc, char **argv)
{
    cxxopts::Options options(argv[0], "Testing file watcher");

    // clang-format off
    options.add_options()
        ("h,help", "Prints usage")
        ("p,path", "Watch path for changes", cxxopts::value<std::string>())
        ("d,depth", "Depth to watch files", cxxopts::value<size_t>()->default_value("10"))
        ("t,time", "Execute x times per seconds", cxxopts::value<double>()->default_value("0.5"));
    // clang-format on

    cxxopts::ParseResult result = options.parse(argc, argv);

    if (result.count("help"))
    {
        fmt::print(options.help());
        return 0;
    }

    LoggerManager::console();
    LoggerManager::thrower(LogLevel::error);
    Logger log("demo");

    Handler<FileWatcher *> handler([&log](FileWatcher *fw) {
        log.debug(fmt::format("watching '{}' with {} files", fw->watch_path(), fw->watch_size()));

        if (!fw->created().empty())
            log.notice(display_fw("created", fw->created()));
        if (!fw->removed().empty())
            log.warning(display_fw("removed", fw->removed()));
        if (!fw->changed().empty())
            log.info(display_fw("changed", fw->changed()));
    });

    const std::string & watch_path = result["path"].as<std::string>();

    FileWatcher watcher("file-watcher");

    watcher.set_path(watch_path);
    watcher.set_max_depth(result["depth"].as<size_t>());
    watcher.set_polling_frequency(result["time"].as<double>());

    watcher.add_observer(&handler);

    watcher.set_start_synchronised(true);
    watcher.start();

    SigWaiter waiter;

    log.info(fmt::format("stopped watching: {} files", watcher.watch_size()));

    return 0;
}