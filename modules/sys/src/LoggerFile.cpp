#include <sihd/sys/LoggerFile.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;
namespace sihd::sys
{

SIHD_LOGGER;

LoggerFile::LoggerFile(const std::string & path, bool append)
{
    _file.buffering_line();
    SIHD_UNEXPECTED_LOG(_file.open(path, append ? "a" : "w"));
}

LoggerFile::~LoggerFile() = default;

void LoggerFile::log(const LogInfo & info, std::string_view msg)
{
    if (!_file.is_open())
        return;
    (void)_file.write_unlocked(info.format(msg));
}

} // namespace sihd::sys
