#include <sihd/util/LoggerFilter.hpp>

namespace sihd::util
{

LoggerFilter::LoggerFilter(const Options & options): _options(options)
{
    if (_options.message_regex.empty() == false)
        _message_regex = std::regex(_options.message_regex);
    if (_options.source_regex.empty() == false)
        _source_regex = std::regex(_options.source_regex);
    if (_options.thread_regex.empty() == false)
        _thread_regex = std::regex(_options.thread_regex);
}

LoggerFilter::~LoggerFilter() = default;

const LoggerFilter::Options & LoggerFilter::options() const
{
    return _options;
}

bool LoggerFilter::filter(const LogInfo & info)
{
    if (_source_regex && std::regex_match(info.source.begin(), info.source.end(), *_source_regex))
        return true;
    if (_thread_regex && std::regex_match(info.thread_name.begin(), info.thread_name.end(), *_thread_regex))
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

bool LoggerFilter::filter(const LogInfo & info, std::string_view msg)
{
    (void)info;
    if (_message_regex && std::regex_match(msg.begin(), msg.end(), *_message_regex))
        return true;
    return false;
}

} // namespace sihd::util
