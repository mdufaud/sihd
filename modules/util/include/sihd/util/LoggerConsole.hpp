#ifndef __SIHD_UTIL_LOGGERCONSOLE_HPP__
#define __SIHD_UTIL_LOGGERCONSOLE_HPP__

#include <sihd/util/ALogger.hpp>
#include <sihd/util/term.hpp>

namespace sihd::util
{

class LoggerConsole: public ALogger
{
    public:
        LoggerConsole(const std::string & name, Node *parent = nullptr);
        // explicit: a string literal would otherwise rank const char*->bool above ->std::string
        explicit LoggerConsole(bool colors = term::supports_color(stderr));
        virtual ~LoggerConsole();

        virtual void log(const LogInfo & info, std::string_view msg) override;

        bool colors() const { return _colors; }
        bool set_colors(bool colors);

    private:
        bool _colors;
};

} // namespace sihd::util

#endif
