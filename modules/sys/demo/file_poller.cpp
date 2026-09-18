#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/FilePoller.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::sys;

SIHD_NEW_LOGGER("demo");

namespace
{

std::string display_fw(std::string_view category, const std::vector<std::string> & list)
{
    constexpr size_t max_width = 200;
    return str::wrap(fmt::format("{} [{}]: {}", category, list.size(), fmt::join(list, ",")), max_width);
}

} // namespace

int main(int argc, char **argv)
{
    App app({
        .name = "file_poller",
        .description = "Testing file poller",
    });

    std::string path;
    size_t depth = 10;
    double time_val = 0.5;
    app.root().bind("path", path, "Watch path for changes", "p");
    app.root().bind("depth", depth, "Depth to watch files", "d");
    app.root().bind("time", time_val, "Execute x times per seconds", "t");

    app.root().on_run([&] {
        if (path.empty())
        {
            SIHD_LOG(error, "Missing --path");
            app.exit(EXIT_FAILURE);
        }

        Handler<FilePoller *> handler([](FilePoller *fw) {
            SIHD_LOG(debug, "watching '{}' with {} files", fw->watch_path(), fw->watch_size());

            if (!fw->created().empty())
                SIHD_LOG(notice, "{}", display_fw("created", fw->created()));
            if (!fw->removed().empty())
                SIHD_LOG(warning, "{}", display_fw("removed", fw->removed()));
            if (!fw->changed().empty())
                SIHD_LOG(info, "{}", display_fw("changed", fw->changed()));
        });

        const Timestamp sleep_for = time::from_double(time_val);
        FilePoller fpoller(path, depth);
        fpoller.add_observer(&handler);

        while (app.should_stop() == false)
        {
            fpoller.run();
            app.wait_for_termination(Duration(sleep_for));
        }

        SIHD_LOG(info, "stopped watching: {} files", fpoller.watch_size());
    });

    return app.run(argc, argv);
}
