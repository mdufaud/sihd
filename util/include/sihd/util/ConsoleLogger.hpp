#ifndef __SIHD_UTIL_CONSOLELOGGER_HPP__
#define __SIHD_UTIL_CONSOLELOGGER_HPP__

#include <sihd/util/ALogger.hpp>

namespace sihd::util
{

class ConsoleLogger: public ALogger
{
    public:
        ConsoleLogger();
        virtual ~ConsoleLogger();

        virtual void log(const LogInfo & info, std::string_view msg) override;

    protected:

    private:
};

} // namespace sihd::util

#endif