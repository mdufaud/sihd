#ifndef __SIHD_UTIL_THROWLOGGER_HPP__
#define __SIHD_UTIL_THROWLOGGER_HPP__

#include <exception>

#include <sihd/util/ALogger.hpp>

namespace sihd::util
{

class ThrowLogger: public ALogger
{
    public:
        class Exception: public std::exception
        {
            public:
                Exception(const LogInfo & info, std::string_view msg);

                const LogInfo & log_info() const;
                virtual const char *what() const noexcept;

            private:
                LogInfo _log_info;
                std::string _msg;
        };

        ThrowLogger();
        virtual ~ThrowLogger();

        virtual void log(const LogInfo & info, std::string_view msg) override;

    protected:

    private:
};

} // namespace sihd::util

#endif
