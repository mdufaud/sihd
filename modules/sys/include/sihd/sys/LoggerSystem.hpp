#ifndef __SIHD_SYS_LOGGERSYSTEM_HPP__
#define __SIHD_SYS_LOGGERSYSTEM_HPP__

#include <expected>
#include <string>

#include <sihd/sys/platform.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::sys
{

// syslog on linux is a process-global connection: opened by the first
// instance, closed by the last; windows holds a per-instance event source
class LoggerSystem: public sihd::util::ALogger
{
    public:
        // default syslog facility/options (linux); meaningless on windows event log
        static constexpr int default_facility = 1;   // syslog.h LOG_USER
        static constexpr int default_options = 0x09; // syslog.h LOG_NDELAY | LOG_PID

        LoggerSystem(const std::string & name, sihd::util::Node *parent = nullptr);
        LoggerSystem(std::string_view progname, int facility, int options);
        ~LoggerSystem();

        void log(const sihd::util::LogInfo & info, std::string_view msg) override;

        bool set_facility(int facility);

    private:
        std::expected<void, sihd::util::Error> _open_source();

        std::string _progname;
        int _facility = default_facility;
        int _options = default_options;
        struct Impl;
        Impl *_impl = nullptr;
};

} // namespace sihd::sys

#endif
