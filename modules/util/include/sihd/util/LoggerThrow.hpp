#ifndef __SIHD_UTIL_LOGGERTHROW_HPP__
#define __SIHD_UTIL_LOGGERTHROW_HPP__

#include <exception>

#include <sihd/util/ALogger.hpp>

namespace sihd::util
{

class LoggerThrow: public ALogger
{
    public:
        class Exception: public std::exception
        {
            public:
                Exception(const LogInfo & info, std::string_view msg);

                const LogInfo & log_info() const;
                const char *what() const noexcept;

            private:
                LogInfo _log_info;
                std::string _msg;
        };

        LoggerThrow() = default;
        ~LoggerThrow() = default;

        void log(const LogInfo & info, std::string_view msg) override;

    protected:

    private:
};

} // namespace sihd::util

#endif
