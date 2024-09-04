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

    if (print_thread_id)
    {
        fmt_msg = fmt::format("{0}.{1}\t{2}\t[{3}]\t{4:<9} {5}\t{6}\n",
                              info.timespec.tv_sec,
                              info.timespec.tv_nsec,
                              info.thread_id_str.c_str(),
                              info.thread_name.data(),
                              info.strlevel,
                              info.source.data(),
                              msg.data());
    }
    else
    {
        fmt_msg = fmt::format("{0}.{1}\t[{2}]\t{3:<9} {4}\t{5}\n",
                              info.timespec.tv_sec,
                              info.timespec.tv_nsec,
                              info.thread_name.data(),
                              info.strlevel,
                              info.source.data(),
                              msg.data());
    }
    fwrite(fmt_msg.c_str(), sizeof(char), fmt_msg.size(), _output);
}

} // namespace sihd::util
