#include <stdexcept>

#include <fmt/format.h>

#include <sihd/sys/LoggerFile.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

using namespace sihd::util;
namespace sihd::sys
{

SIHD_LOGGER;

LoggerFile::LoggerFile(const std::string & name, sihd::util::Node *parent): ALogger(name, parent)
{
    this->add_conf("path", &LoggerFile::set_path);
    this->add_conf("append", &LoggerFile::set_append);
    this->add_conf("pattern", &LoggerFile::set_pattern);
}

LoggerFile::LoggerFile(const std::string & path, bool append, const std::string & pattern): ALogger(path)
{
    _append = append;
    _path = path;
    auto opened = this->_open_file();
    if (!opened)
        throw std::runtime_error(std::move(opened).error().message);
    if (!this->set_pattern(pattern))
        throw std::runtime_error(fmt::format("cannot set log pattern '{}'", pattern));
}

LoggerFile::~LoggerFile() = default;

bool LoggerFile::set_path(const std::string & path)
{
    _path = path;
    return this->_open_file().has_value();
}

bool LoggerFile::set_append(bool append)
{
    if (_append == append)
        return true;
    _append = append;
    if (_file.is_open())
        return this->_open_file().has_value();
    return true;
}

bool LoggerFile::set_pattern(const std::string & pattern)
{
    if (pattern.empty())
        return true;
    auto patterned = _formatter.set_pattern(pattern);
    return patterned.has_value();
}

std::expected<void, Error> LoggerFile::_open_file()
{
    return _file.open(_path, _append ? "a" : "w");
}

void LoggerFile::log(const LogInfo & info, std::string_view msg)
{
    if (!_file.is_open())
    {
        if (_warned_unconfigured == false)
        {
            _warned_unconfigured = true;
            SIHD_LOG(warning, "no path configured, dropping messages");
        }
        return;
    }
    (void)_file.write_unlocked(_formatter.format(info, msg));
}

SIHD_REGISTER_FACTORY(LoggerFile);

} // namespace sihd::sys
