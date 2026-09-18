#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/sys/App.hpp>
#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

using namespace sihd::util;
using namespace sihd::sys;

SIHD_NEW_LOGGER("demo");

void do_dump_env(bool do_dump, const std::vector<std::string> & env)
{
    if (do_dump)
    {
        if (env.size() > 0)
        {
            SIHD_LOG(info, "env: ");
            for (const std::string & env : env)
            {
                SIHD_LOG(info, "  {}", env);
            }
        }
    }
    else if (env.size() > 0)
        SIHD_LOG(info, "env: {} ...", env.front());
}

int main(int argc, char **argv)
{
    if constexpr (sihd::util::build::is_windows)
    {
        term::set_output_utf8();
    }

    App app({
        .name = "process_info",
        .description = "Testing process info",
    });

    std::string process_name;
    int id = -1;
    bool dump_env = false;
    app.root().bind("process", process_name, "Dump process info by name", "p");
    app.root().bind("id", id, "Dump process info by id", "i");
    app.root().bind("env", dump_env, "Dump process full env", "e");

    app.root().on_run([&] {
        if (process_name.empty() == false)
        {
            for (const ProcessInfo & process : ProcessInfo::get_all_process_from_name(process_name))
            {
                SIHD_LOG(info, "pid: {}", process.pid());
                SIHD_LOG(info, "name: {}", process.name());
                SIHD_LOG(info, "cwd: {}", process.cwd());
                if (process.creation_time() != 0)
                {
                    SIHD_LOG(info, "creation_time: {}", process.creation_time().local_str());
                }
                SIHD_LOG(info, "exe_path: {}", process.exe_path());
                SIHD_LOG(info, "cmd_line: {}", fmt::join(process.cmd_line(), " "));
                do_dump_env(dump_env, process.env());
                SIHD_LOG(notice, "---");
            }
        }
        else if (id != -1)
        {
            ProcessInfo process(id);
            SIHD_LOG(info, "pid: {}", process.pid());
            SIHD_LOG(info, "name: {}", process.name());
            SIHD_LOG(info, "cwd: {}", process.cwd());
            SIHD_LOG(info, "exe_path: {}", process.exe_path());
            SIHD_LOG(info, "cmd_line: {}", fmt::join(process.cmd_line(), " "));
            do_dump_env(dump_env, process.env());
        }
    });

    return app.run(argc, argv);
}
