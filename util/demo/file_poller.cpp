#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

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
    std::string path;
    size_t depth = 10;
    double time_val = 0.5;

    CLI::App app {"Testing file poller"};
    app.add_option("-p,--path", path, "Watch path for changes")->required();
    app.add_option("-d,--depth", depth, "Depth to watch files")->default_val("10");
    app.add_option("-t,--time", time_val, "Execute x times per seconds")->default_val("0.5");
    app.add_option("path", path);
    app.add_option("depth", depth);

    CLI11_PARSE(app, argc, argv);

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

    const Timestamp sleep_for = time::from_double(time_val);
    FilePoller fpoller(path, depth);

    fpoller.add_observer(&handler);

    SigHandler sig_handler(SIGINT);
    while (!signal::termination_received())
    {
        time::sleep_t(sleep_for);
        fpoller.run();
    }

    log.info(fmt::format("stopped watching: {} files", fpoller.watch_size()));

    return 0;
}