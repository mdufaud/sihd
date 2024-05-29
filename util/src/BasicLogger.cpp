#include <fmt/printf.h>

#include <sihd/util/BasicLogger.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util
{

BasicLogger::BasicLogger(FILE *output, bool print_thread_id): print_thread_id(print_thread_id), _output(output) {}

BasicLogger::~BasicLogger() {}

void BasicLogger::log(const LogInfo & info, std::string_view msg)
{
    std::string fmt_msg;

// SEC.NANO [THREAD] LEVEL SRC MSG
#if defined(__SIHD_WINDOWS__)
    fmt_msg = fmt::sprintf("%lld.%09ld\t%s[%s]\t%s\t%s\t%s\n",
#else
    fmt_msg = fmt::sprintf("%ld.%09ld\t%s[%s]\t%s\t%s\t%s\n",
#endif
                           info.timespec.tv_sec,
                           info.timespec.tv_nsec,
                           print_thread_id ? info.thread_id_str.c_str() : "",
                           info.thread_name.data(),
                           info.strlevel,
                           info.source.data(),
                           msg.data());
    fwrite(fmt_msg.c_str(), sizeof(char), fmt_msg.size(), _output);
}

} // namespace sihd::util
