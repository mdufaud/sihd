#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>

namespace sihd::util
{

SIHD_LOGGER;

LoggerStream::LoggerStream(FILE *output, const std::string & pattern): _output(output)
{
    if (pattern.empty() == false && SIHD_UNEXPECTED_LOG(_formatter.set_pattern(pattern)))
        return;
}

LoggerStream::~LoggerStream() = default;

void LoggerStream::log(const LogInfo & info, std::string_view msg)
{
    const std::string line = _formatter.format(info, msg);
    fwrite(line.c_str(), sizeof(char), line.size(), _output);
}

} // namespace sihd::util
