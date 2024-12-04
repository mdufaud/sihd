#ifndef __SIHD_UTIL_LOGGERFILE_HPP__
#define __SIHD_UTIL_LOGGERFILE_HPP__

#include <sihd/util/ALogger.hpp>
#include <sihd/util/File.hpp>

namespace sihd::util
{

class LoggerFile: public ALogger
{
    public:
        LoggerFile(const std::string & path, bool append = true);
        virtual ~LoggerFile();

        virtual void log(const LogInfo & info, std::string_view msg) override;

        bool is_open() const { return _file.is_open(); }
        const std::string & path() const { return _file.path(); }

    protected:

    private:
        File _file;
};

} // namespace sihd::util

#endif