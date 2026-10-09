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
        LoggerStream(const std::string & name, Node *parent = nullptr);
        LoggerStream(FILE *output = stderr, const std::string & pattern = "");
        ~LoggerStream();

        void log(const LogInfo & info, std::string_view msg) override;

        bool set_pattern(const std::string & pattern);

    private:
        FILE *_output = stderr;
        LogFormatter _formatter;
};

} // namespace sihd::util

#endif
