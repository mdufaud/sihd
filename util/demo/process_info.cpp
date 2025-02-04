#include <cxxopts.hpp>
#include <fmt/format.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/ProcessInfo.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

using namespace sihd::util;

SIHD_NEW_LOGGER("demo");

int main(int argc, char **argv)
{
    if constexpr (os::is_windows)
    {
        term::set_output_utf8();
    }

    cxxopts::Options options(argv[0], "Testing process info");

    // clang-format off
    options.add_options()
        ("h,help", "Prints usage")
        ("p,process", "Dump process info by name", cxxopts::value<std::string>())
        ("i,id", "Dump process info by id", cxxopts::value<int>());
    // clang-format on

    cxxopts::ParseResult result = options.parse(argc, argv);

    if (result.count("help"))
    {
        fmt::print("{}\n", options.help());
        return 0;
    }

    LoggerManager::console();

    if (result.count("process"))
    {
        for (const ProcessInfo & process : ProcessInfo::get_all_process_from_name(result["process"].as<std::string>()))
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
            if (process.env().size() > 0)
                SIHD_LOG(info, "env: {} ...", process.env().front());
            SIHD_LOG(notice, "---");
        }
    }
    else if (result.count("id"))
    {
        ProcessInfo process(result["id"].as<int>());
        SIHD_LOG(info, "pid: {}", process.pid());
        SIHD_LOG(info, "name: {}", process.name());
        SIHD_LOG(info, "cwd: {}", process.cwd());
        SIHD_LOG(info, "exe_path: {}", process.exe_path());
        SIHD_LOG(info, "cmd_line: {}", fmt::join(process.cmd_line(), " "));
        SIHD_LOG(info, "env: ");
        for (const std::string & env : process.env())
        {
            SIHD_LOG(info, "  {}", env);
        }
    }

    return 0;
}