#ifndef __SIHD_SYS_LOGGERFILE_HPP__
#define __SIHD_SYS_LOGGERFILE_HPP__

#include <expected>
#include <string>

#include <sihd/sys/File.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/LogFormatter.hpp>

namespace sihd::sys
{

// an empty pattern keeps the default layout - a refused pattern falls back to it
class LoggerFile: public sihd::util::ALogger
{
    public:
        LoggerFile(const std::string & name, sihd::util::Node *parent = nullptr);
        LoggerFile(const std::string & path, bool append, const std::string & pattern);
        virtual ~LoggerFile();

        virtual void log(const sihd::util::LogInfo & info, std::string_view msg) override;

        bool is_open() const { return _file.is_open(); }
        const std::string & path() const { return _path; }

        // the path setter opens the file, the append setter reopens it when already open
        bool set_path(const std::string & path);
        bool set_append(bool append);
        bool set_pattern(const std::string & pattern);

    private:
        std::expected<void, sihd::util::Error> _open_file();

        std::string _path;
        bool _append = true;
        bool _warned_unconfigured = false;
        File _file;
        sihd::util::LogFormatter _formatter;
};

} // namespace sihd::sys

#endif
