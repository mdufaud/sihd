#include <cxxopts.hpp>
#include <fmt/format.h>

#include <sihd/util/FilePoller.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/signal.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;

std::string display_fw(std::string_view category, const std::vector<std::string> & list)
{
    constexpr size_t max_width = 200;
    return str::wrap(fmt::format("{} [{}]: {}", category, list.size(), fmt::join(list, ",")), max_width);
}

int main(int argc, char **argv)
{
    cxxopts::Options options(argv[0], "Testing file poller");

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
        fmt::print("{}\n", options.help());
        return 0;
    }

    LoggerManager::console();
    Logger log("demo");

    Handler<FilePoller *> handler([&log](FilePoller *fw) {
        log.debug(fmt::format("watching '{}' with {} files", fw->watch_path(), fw->watch_size()));

        if (!fw->created().empty())
            log.notice(display_fw("created", fw->created()));
        if (!fw->removed().empty())
            log.warning(display_fw("removed", fw->removed()));
        if (!fw->changed().empty())
            log.info(display_fw("changed", fw->changed()));
    });

    const Timestamp sleep_for = time::from_double(result["time"].as<double>());
    FilePoller fpoller(result["path"].as<std::string>(), result["depth"].as<size_t>());

    fpoller.add_observer(&handler);

    SigHandler sig_handler(SIGINT);
    while (!signal::termination_received())
    {
        time::sleep_t(sleep_for);
        fpoller.check_for_changes();
    }

    log.info(fmt::format("stopped watching: {} files", fpoller.watch_size()));

    return 0;
}