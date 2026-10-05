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

                // copies reseat onto their own strings: assignment is refused rather than dangling
                Exception(const Exception & other);
                Exception(Exception && other);
                Exception & operator=(const Exception &) = delete;

                const LogInfo & log_info() const;
                const char *what() const noexcept;

            private:
                // the LogInfo views dangle once the emitter's thread_local is gone: point them at owned strings
                void _reseat();

                LogInfo _log_info;
                std::string _source;
                std::string _thread_name;
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
