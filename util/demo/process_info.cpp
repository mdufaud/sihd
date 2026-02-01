#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/ProcessInfo.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

using namespace sihd::util;

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
    if constexpr (os::is_windows)
    {
        term::set_output_utf8();
    }

    std::string process_name;
    int id = -1;
    bool dump_env = false;

    CLI::App app {"Testing process info"};
    app.add_option("-p,--process", process_name, "Dump process info by name");
    app.add_option("-i,--id", id, "Dump process info by id");
    app.add_flag("-e,--env", dump_env, "Dump process full env");
    app.add_option("process", process_name);

    CLI11_PARSE(app, argc, argv);

    LoggerManager::console(LoggerFilter::Options {.level_lower = LogLevel::info});

    if (!process_name.empty())
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

    return 0;
}