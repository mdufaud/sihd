#include <fmt/format.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerConsole.hpp>
#include <sihd/util/term.hpp>

namespace sihd::util
{

SIHD_LOGGER;

namespace
{

std::string_view level_str(LogLevel level)
{
    switch (level)
    {
        case LogLevel::emergency:
            return "EMER";
        case LogLevel::alert:
            return "A";
        case LogLevel::critical:
            return "C";
        case LogLevel::error:
            return "E";
        case LogLevel::warning:
            return "W";
        case LogLevel::notice:
            return "N";
        case LogLevel::info:
            return "I";
        case LogLevel::debug:
            return "D";
        default:
            return "-";
    }
}

const char *level_color(LogLevel level)
{
    switch (level)
    {
        case LogLevel::emergency:
            return term::attr::VIOLET;
        case LogLevel::alert:
            return term::attr::VIOLET;
        case LogLevel::critical:
            return term::attr::VIOLET2;
        case LogLevel::error:
            return term::attr::RED2;
        case LogLevel::warning:
            return term::attr::YELLOW;
        case LogLevel::notice:
            return term::attr::CYAN;
        case LogLevel::info:
            return term::attr::GREEN;
        case LogLevel::debug:
            return term::attr::GREY;
        default:
            return term::attr::WHITE;
    }
}

} // namespace

LoggerConsole::LoggerConsole(bool colors): _colors(colors) {}

LoggerConsole::~LoggerConsole() = default;

void LoggerConsole::log(const LogInfo & info, std::string_view msg)
{
    const char *beg = _colors ? level_color(info.level) : "";
    const char *end = _colors ? term::attr::ENDC : "";

    const std::string fmt_msg = fmt::format("{}{} [{}] <{}> {}{}\n",
                                            beg,
                                            level_str(info.level),
                                            info.thread_name.data(),
                                            info.source.data(),
                                            msg,
                                            end);

    fwrite(fmt_msg.c_str(), sizeof(char), fmt_msg.size(), stderr);
}

} // namespace sihd::util
