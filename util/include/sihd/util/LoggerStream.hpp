#ifndef __SIHD_UTIL_LOGGERSTREAM_HPP__
#define __SIHD_UTIL_LOGGERSTREAM_HPP__

#include <sihd/util/ALogger.hpp>

namespace sihd::util
{

class LoggerStream: public ALogger
{
    public:
        LoggerStream(FILE *output = stderr, bool print_thread_id = false);
        ~LoggerStream();

        void log(const LogInfo & info, std::string_view msg) override;

        bool print_thread_id;

    private:
        FILE *_output;
};

} // namespace sihd::util

#endif