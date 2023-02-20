#ifndef __SIHD_UTIL_FILELOGGER_HPP__
#define __SIHD_UTIL_FILELOGGER_HPP__

#include <sihd/util/ALogger.hpp>
#include <sihd/util/File.hpp>

namespace sihd::util
{

class FileLogger: public ALogger
{
    public:
        FileLogger(const std::string & path, bool append = true);
        virtual ~FileLogger();

        virtual void log(const LogInfo & info, std::string_view msg) override;

        bool is_open() const { return _file.is_open(); }
        const std::string & path() const { return _file.path(); }

    protected:

    private:
        File _file;
};

} // namespace sihd::util

#endif