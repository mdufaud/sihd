#ifndef __SIHD_UTIL_LOGGERCONSOLE_HPP__
#define __SIHD_UTIL_LOGGERCONSOLE_HPP__

#include <sihd/util/ALogger.hpp>
#include <sihd/util/term.hpp>

namespace sihd::util
{

class LoggerConsole: public ALogger
{
    public:
        LoggerConsole(bool colors = term::supports_color(stderr));
        virtual ~LoggerConsole();

        virtual void log(const LogInfo & info, std::string_view msg) override;

        bool colors() const { return _colors; }

    private:
        bool _colors;
};

} // namespace sihd::util

#endif
