#include <fmt/printf.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerFile.hpp>

namespace sihd::util
{

LoggerFile::LoggerFile(const std::string & path, bool append)
{
    _file.buffering_line();
    _file.open(path, append ? "a" : "w");
}

LoggerFile::~LoggerFile() = default;

void LoggerFile::log(const LogInfo & info, std::string_view msg)
{
    if (!_file.is_open())
        return;

    std::string fmt_msg;

// SEC.NANO [THREAD] LEVEL SRC MSG
#if defined(__SIHD_WINDOWS__)
    fmt_msg = fmt::sprintf("%lld.%09ld\t[%s]\t%s\t%s\t%s\n",
#else
    fmt_msg = fmt::sprintf("%ld.%09ld\t[%s]\t%s\t%s\t%s\n",
#endif
                           info.timespec.tv_sec,
                           info.timespec.tv_nsec,
                           info.thread_name.data(),
                           info.strlevel,
                           info.source.data(),
                           msg);

    _file.write_unlocked(fmt_msg);
}

} // namespace sihd::util
