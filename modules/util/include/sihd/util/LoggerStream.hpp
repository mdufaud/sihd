#ifndef __SIHD_UTIL_LOGGERSTREAM_HPP__
#define __SIHD_UTIL_LOGGERSTREAM_HPP__

#include <string>

#include <sihd/util/ALogger.hpp>
#include <sihd/util/LogFormatter.hpp>

namespace sihd::util
{

// an empty pattern keeps the default layout - a refused pattern falls back to it
class LoggerStream: public ALogger
{
    public:
        LoggerStream(FILE *output = stderr, const std::string & pattern = "");
        ~LoggerStream();

        void log(const LogInfo & info, std::string_view msg) override;

    private:
        FILE *_output;
        LogFormatter _formatter;
};

} // namespace sihd::util

#endif
