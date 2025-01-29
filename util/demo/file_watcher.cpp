#include <cxxopts.hpp>
#include <fmt/format.h>

#include <sihd/util/FileWatcher.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/signal.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;

int main(int argc, char **argv)
{
    cxxopts::Options options(argv[0], "Testing file watcher");

    // clang-format off
    options.add_options()
        ("h,help", "Prints usage")
        ("p,path", "Watch path for changes", cxxopts::value<std::string>());
    // clang-format on

    cxxopts::ParseResult result = options.parse(argc, argv);

    if (result.count("help"))
    {
        fmt::print("{}\n", options.help());
        return 0;
    }

    LoggerManager::console();
    Logger log("demo");

    const std::string & watch_path = result["path"].as<std::string>();

    FileWatcher fw(watch_path, [](FileWatcherEvent) {});

    SigHandler sig_handler(SIGINT);
    while (!signal::termination_received())
    {
        fw.poll();
    }

    return 0;
}