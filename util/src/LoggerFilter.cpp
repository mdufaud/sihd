#include <sihd/util/LoggerFilter.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

LoggerFilter::LoggerFilter(const Options & options): _options(options) {}

LoggerFilter::~LoggerFilter() {}

bool LoggerFilter::filter(const LogInfo & info, [[maybe_unused]] std::string_view msg)
{
    if (!_options.message_regex.empty() && str::regex_match(msg, _options.message_regex))
        return true;
    if (!_options.source_regex.empty() && str::regex_match(info.source, _options.source_regex))
        return true;
    if (!_options.thread_regex.empty() && str::regex_match(info.thread_name, _options.thread_regex))
        return true;

    if (_options.thread_eq != 0 && info.thread_id == _options.thread_eq)
        return true;
    if (_options.thread_ne != 0 && info.thread_id != _options.thread_ne)
        return true;

    if (_options.level_eq != LogLevel::none && info.level == _options.level_eq)
        return true;
    if (_options.level_higher != LogLevel::none && info.level < _options.level_higher)
        return true;
    if (_options.level_lower != LogLevel::none && info.level > _options.level_lower)
        return true;

    return false;
}

} // namespace sihd::util
