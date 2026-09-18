#include <string>

#include <fmt/format.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/FileWatcher.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

using namespace sihd::util;
using namespace sihd::sys;

SIHD_NEW_LOGGER("demo");

int main(int argc, char **argv)
{
    if constexpr (sihd::util::build::is_windows)
    {
        term::set_output_utf8();
    }

    App app({
        .name = "file_watcher",
        .description = "Testing file watcher",
    });

    std::string path;
    app.root().bind("path", path, "Watch path for changes", "p");

    app.root().on_run([&] {
        if (path.empty())
        {
            SIHD_LOG(error, "Missing --path");
            app.exit(EXIT_FAILURE);
        }

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

        FileWatcher fw;
        if (fw.watch(path) == false)
        {
            SIHD_LOG(error, "Failed to watch: {}", path);
            app.exit(EXIT_FAILURE);
        }
        constexpr int timeout_milliseconds = 500;
        fw.set_run_timeout(timeout_milliseconds);
        fw.add_observer(&handler);

        while (stop == false && app.should_stop() == false)
        {
            fw.run();
        }
    });

    return app.run(argc, argv);
}
