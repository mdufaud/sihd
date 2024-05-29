#include <fmt/format.h>

#include <sihd/util/ConsoleLogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

namespace sihd::util
{

SIHD_LOGGER;

ConsoleLogger::ConsoleLogger() {}

ConsoleLogger::~ConsoleLogger() {}

void ConsoleLogger::log(const LogInfo & info, std::string_view msg)
{
    const char *beg;
    const char *level;
    const char *end = term::attr::ENDC;

    switch (info.level)
    {
        case LogLevel::emergency:
            beg = term::attr::VIOLET;
            level = "EMER";
            break;
        case LogLevel::alert:
            beg = term::attr::VIOLET;
            level = "A";
            break;
        case LogLevel::critical:
            beg = term::attr::VIOLET2;
            level = "C";
            break;
        case LogLevel::error:
            beg = term::attr::RED2;
            level = "E";
            break;
        case LogLevel::warning:
            beg = term::attr::YELLOW;
            level = "W";
            break;
        case LogLevel::notice:
            beg = term::attr::CYAN;
            level = "N";
            break;
        case LogLevel::info:
            beg = term::attr::GREEN;
            level = "I";
            break;
        case LogLevel::debug:
            beg = term::attr::GREY;
            level = "D";
            break;
        default:
            level = "-";
            beg = term::attr::WHITE;
    }

    const std::string fmt_msg = fmt::format("{}{} [{}] <{}> {}{}\n",
                                            beg,
                                            level,
                                            info.thread_name.data(),
                                            info.source.data(),
                                            msg.data(),
                                            end);

    fwrite(fmt_msg.c_str(), sizeof(char), fmt_msg.size(), stderr);
}

} // namespace sihd::util
