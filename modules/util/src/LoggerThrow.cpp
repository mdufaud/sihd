#include <sihd/util/LoggerThrow.hpp>

namespace sihd::util
{

LoggerThrow::Exception::Exception(const LogInfo & info, std::string_view msg):
    _log_info(info),
    _source(info.source),
    _thread_name(info.thread_name),
    _msg(msg)
{
    this->_reseat();
}

LoggerThrow::Exception::Exception(const Exception & other): Exception(other._log_info, other._msg) {}

LoggerThrow::Exception::Exception(Exception && other): Exception(other._log_info, other._msg) {}

void LoggerThrow::Exception::_reseat()
{
    this->_log_info.source = this->_source;
    this->_log_info.thread_name = this->_thread_name;
}

const char *LoggerThrow::Exception::what() const noexcept
{
    return _msg.c_str();
}

const LogInfo & LoggerThrow::Exception::log_info() const
{
    return _log_info;
}

void LoggerThrow::log([[maybe_unused]] const LogInfo & info, std::string_view msg)
{
    throw Exception(info, msg);
}

} // namespace sihd::util
