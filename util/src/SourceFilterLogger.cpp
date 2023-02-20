#include <sihd/util/SourceFilterLogger.hpp>

namespace sihd::util
{

SourceFilterLogger::SourceFilterLogger(const std::string & source, bool match_only):
    source(source),
    match_only(match_only)
{
}

SourceFilterLogger::~SourceFilterLogger() {}

bool SourceFilterLogger::filter(const LogInfo & info, std::string_view msg)
{
    (void)msg;
    return this->match_only ? info.source.find(this->source) == std::string::npos
                            : info.source.find(this->source) != std::string::npos;
}

} // namespace sihd::util