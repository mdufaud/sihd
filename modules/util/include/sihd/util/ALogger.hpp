#ifndef __SIHD_UTIL_ALOGGER_HPP__
#define __SIHD_UTIL_ALOGGER_HPP__

#include <sihd/util/ALogFilterer.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/ILoggerFilter.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::util
{

class ALogger: public ALogFilterer,
               public Named,
               public Configurable
{
    public:
        ALogger(const std::string & name, Node *parent = nullptr);
        virtual ~ALogger() = default;

        virtual void log(const LogInfo & info, std::string_view msg) = 0;
};

} // namespace sihd::util

#endif
