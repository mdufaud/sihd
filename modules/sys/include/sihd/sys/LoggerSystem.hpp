#ifndef __SIHD_SYS_LOGGERSYSTEM_HPP__
#define __SIHD_SYS_LOGGERSYSTEM_HPP__

#include <memory>

#include <sihd/sys/platform.hpp>
#include <sihd/util/ALogger.hpp>

namespace sihd::sys
{

class LoggerSystem: public sihd::util::ALogger
{
    public:
        // default syslog facility/options (linux); meaningless on windows event log
        static constexpr int default_facility = 1; // syslog.h LOG_USER
        static constexpr int default_options = 0x09; // syslog.h LOG_NDELAY | LOG_PID

        LoggerSystem(std::string_view progname, int facility = default_facility, int options = default_options);
        ~LoggerSystem();

        void log(const sihd::util::LogInfo & info, std::string_view msg) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
