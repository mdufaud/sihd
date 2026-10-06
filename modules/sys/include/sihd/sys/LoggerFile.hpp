#ifndef __SIHD_SYS_LOGGERFILE_HPP__
#define __SIHD_SYS_LOGGERFILE_HPP__

#include <sihd/sys/File.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/LogFormatter.hpp>

namespace sihd::sys
{

// an empty pattern keeps the default layout - a refused pattern falls back to it
class LoggerFile: public sihd::util::ALogger
{
    public:
        LoggerFile(const std::string & path, bool append = true, const std::string & pattern = "");
        virtual ~LoggerFile();

        virtual void log(const sihd::util::LogInfo & info, std::string_view msg) override;

        bool is_open() const { return _file.is_open(); }
        const std::string & path() const { return _file.path(); }

    protected:

    private:
        File _file;
        sihd::util::LogFormatter _formatter;
};

} // namespace sihd::sys

#endif