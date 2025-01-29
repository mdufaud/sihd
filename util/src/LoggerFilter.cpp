#include <sihd/util/LoggerFilter.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

LoggerFilter::LoggerFilter(const Options & options): _options(options) {}

LoggerFilter::~LoggerFilter() {}

bool LoggerFilter::filter(const LogInfo & info, [[maybe_unused]] std::string_view msg)
{
    // if (!_options.message_regex.empty() && str::regex_match(msg, _options.message_regex))
    // {
    //     return false;
    // }
    // if (!_options.source_regex.empty() && str::regex_match(info.source, _options.source_regex))
    // {
    //     return false;
    // }
    // if (!_options.thread_regex.empty() && str::regex_match(info.thread_id_str, _options.thread_regex))
    // {
    //     return false;
    // }

    if (_options.thread_eq != 0 && info.thread_id == _options.thread_eq)
    {
        return false;
    }
    if (_options.thread_ne != 0 && info.thread_id != _options.thread_ne)
    {
        return false;
    }

    if (_options.level_eq != LogLevel::none && info.level == _options.level_eq)
    {
        return false;
    }
    if (_options.level_gt != LogLevel::none && info.level > _options.level_gt)
    {
        return false;
    }
    if (_options.level_lt != LogLevel::none && info.level < _options.level_lt)
    {
        return false;
    }

    return true;
}

} // namespace sihd::util
