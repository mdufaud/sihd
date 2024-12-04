#include <sihd/util/LoggerFilterSource.hpp>

namespace sihd::util
{

LoggerFilterSource::LoggerFilterSource(const std::string & source, bool match_only):
    source(source),
    match_only(match_only)
{
}

LoggerFilterSource::~LoggerFilterSource() {}

bool LoggerFilterSource::filter(const LogInfo & info, std::string_view msg)
{
    (void)msg;
    return this->match_only ? info.source.find(this->source) == std::string::npos
                            : info.source.find(this->source) != std::string::npos;
}

} // namespace sihd::util