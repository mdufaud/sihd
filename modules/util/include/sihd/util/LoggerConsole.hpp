#ifndef __SIHD_UTIL_LOGGERCONSOLE_HPP__
#define __SIHD_UTIL_LOGGERCONSOLE_HPP__

#include <sihd/util/ALogger.hpp>

namespace sihd::util
{

class LoggerConsole: public ALogger
{
    public:
        LoggerConsole();
        virtual ~LoggerConsole();

        virtual void log(const LogInfo & info, std::string_view msg) override;

    protected:

    private:
};

} // namespace sihd::util

#endif