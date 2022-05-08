#include <sihd/util/ConsoleLogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util
{

SIHD_LOGGER;

ConsoleLogger::ConsoleLogger()
{
}

ConsoleLogger::~ConsoleLogger()
{
}

void    ConsoleLogger::log(const LogInfo & info, std::string_view msg)
{
    const char *beg;
    const char *level;
    const char *end = Term::Attr::ENDC;

    switch (info.level)
    {
        case LogLevel::emergency:
            beg = Term::Attr::VIOLET;
            level = "EMER";
            break ;
        case LogLevel::alert:
            beg = Term::Attr::VIOLET;
            level = "A";
            break ;
        case LogLevel::critical:
            beg = Term::Attr::VIOLET2;
            level = "C";
            break ;
        case LogLevel::error:
            beg = Term::Attr::RED2;
            level = "E";
            break ;
        case LogLevel::warning:
            beg = Term::Attr::YELLOW;
            level = "W";
            break ;
        case LogLevel::notice:
            beg = Term::Attr::CYAN;
            level = "N";
            break ;
        case LogLevel::info:
            beg = Term::Attr::GREEN;
            level = "I";
            break ;
        case LogLevel::debug:
            beg = Term::Attr::GREY;
            level = "D";
            break ;
        default:
            level = "-";
            beg = Term::Attr::WHITE;
    }
    fprintf(stderr, "%s%s [%s] <%s> %s%s\n", beg, level, info.thread_name.data(), info.source.data(), msg.data(), end);
}

}