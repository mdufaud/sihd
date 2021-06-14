#include <sihd/util/SourceFilterLogger.hpp>

namespace sihd::util
{

SourceFilterLogger::SourceFilterLogger(const std::string & source, bool match_only):
    source(source), match_only(match_only)
{
}

SourceFilterLogger::~SourceFilterLogger()
{
}

bool    SourceFilterLogger::filter(const LogInfo & info, const char *msg)
{
    (void)msg;
    return this->match_only
        // Filter if not found
        ? info.source.find(this->source) == std::string::npos
        // Filter if found
        : info.source.find(this->source) != std::string::npos;
}

}