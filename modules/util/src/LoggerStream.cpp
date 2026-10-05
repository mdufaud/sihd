#include <sihd/util/LoggerStream.hpp>

namespace sihd::util
{

LoggerStream::LoggerStream(FILE *output, bool print_thread_id): print_thread_id(print_thread_id), _output(output) {}

LoggerStream::~LoggerStream() = default;

void LoggerStream::log(const LogInfo & info, std::string_view msg)
{
    const std::string line = info.format(msg, print_thread_id);
    fwrite(line.c_str(), sizeof(char), line.size(), _output);
}

} // namespace sihd::util
