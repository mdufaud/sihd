#include <stdexcept>

#include <sihd/util/LoggerFilter.hpp>

namespace sihd::util
{

namespace
{

std::optional<Regex> compile_filter(const std::string & pattern)
{
    if (pattern.empty())
        return std::nullopt;
    Result<Regex> regex = Regex::compile(pattern, Regex::Mode::full);
    if (regex.has_value() == false)
        throw std::invalid_argument(regex.error().message);
    return std::move(*regex);
}

} // namespace

LoggerFilter::LoggerFilter(const Options & options): _options(options)
{
    _message_regex = compile_filter(_options.message_regex);
    _source_regex = compile_filter(_options.source_regex);
    _thread_regex = compile_filter(_options.thread_regex);
}

LoggerFilter::~LoggerFilter() = default;

bool LoggerFilter::filter(const LogInfo & info)
{
    if (_source_regex && _source_regex->matches(info.source).value_or(false))
        return true;
    if (_thread_regex && _thread_regex->matches(info.thread_name).value_or(false))
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
    if (_message_regex && _message_regex->matches(msg).value_or(false))
        return true;
    return false;
}

} // namespace sihd::util
