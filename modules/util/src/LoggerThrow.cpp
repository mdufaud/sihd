
#include <sihd/util/LoggerThrow.hpp>

namespace sihd::util
{

LoggerThrow::Exception::Exception(const LogInfo & info, std::string_view msg): _log_info(info), _msg(msg) {}

const char *LoggerThrow::Exception::what() const noexcept
{
    return _msg.data();
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
