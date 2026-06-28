#ifndef __SIHD_UTIL_ALOGGER_HPP__
#define __SIHD_UTIL_ALOGGER_HPP__

#include <sihd/util/ALogFilterer.hpp>
#include <sihd/util/ILoggerFilter.hpp>
#include <sihd/util/LogInfo.hpp>

namespace sihd::util
{

class ALogger: public ALogFilterer
{
    public:
        ALogger() = default;
        virtual ~ALogger() = default;

        virtual void log(const LogInfo & info, std::string_view msg) = 0;

    private:
};

} // namespace sihd::util

#endif