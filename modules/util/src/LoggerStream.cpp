#include <stdexcept>

#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::util
{

SIHD_LOGGER;

LoggerStream::LoggerStream(const std::string & name, Node *parent): ALogger(name, parent)
{
    this->add_conf("pattern", &LoggerStream::set_pattern);
}

LoggerStream::LoggerStream(FILE *output, const std::string & pattern): ALogger("stream"), _output(output)
{
    if (!pattern.empty())
    {
        auto patterned = _formatter.set_pattern(pattern);
        if (!patterned)
            throw std::runtime_error(std::move(patterned).error().message);
    }
}

LoggerStream::~LoggerStream() = default;

bool LoggerStream::set_pattern(const std::string & pattern)
{
    if (pattern.empty())
        return true;
    auto patterned = _formatter.set_pattern(pattern);
    return patterned.has_value();
}

void LoggerStream::log(const LogInfo & info, std::string_view msg)
{
    const std::string line = _formatter.format(info, msg);
    fwrite(line.c_str(), sizeof(char), line.size(), _output);
}

SIHD_REGISTER_FACTORY(LoggerStream);

} // namespace sihd::util
