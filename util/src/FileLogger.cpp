#include <sihd/util/FileLogger.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

FileLogger::FileLogger(const std::string & path, bool append)
{
    _file.buffering_line();
    _file.open(path, append ? "a" : "w");
}

FileLogger::~FileLogger()
{
}

void    FileLogger::log(const LogInfo & info, const std::string_view & msg)
{
//SEC.NANO [THREAD] LEVEL SRC MSG
#if defined(__SIHD_WINDOWS__)
    fprintf(_file.file(), "%lld.%09ld\t[%s]\t%s\t%s\t%s\n",
# else
    fprintf(_file.file(), "%ld.%09ld\t[%s]\t%s\t%s\t%s\n",
#endif
            info.timestamp.tv_sec, info.timestamp.tv_nsec,
            info.thread_name.data(),
            info.level_str, info.source.data(), msg.data());
}


}