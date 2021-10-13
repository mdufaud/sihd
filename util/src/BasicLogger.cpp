#include <sihd/util/BasicLogger.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util
{

BasicLogger::BasicLogger(FILE *output, bool print_thread_id):
    print_thread_id(print_thread_id), _output(output)
{
}

BasicLogger::~BasicLogger()
{
}

void    BasicLogger::log(const LogInfo & info, const char *msg)
{
//SEC.NANO [THREAD] LEVEL SRC MSG
#if defined(__SIHD_WINDOWS__)
    fprintf(_output, "%lld.%09ld\t%s[%s]\t%s\t%s\t%s\n",
# else
    fprintf(_output, "%ld.%09ld\t%s[%s]\t%s\t%s\t%s\n",
#endif
            info.timestamp.tv_sec, info.timestamp.tv_nsec,
            print_thread_id ? info.thread_id_str.c_str() : "",
            info.thread_name.c_str(),
            info.level_str.c_str(), info.source.c_str(), msg);
}

}