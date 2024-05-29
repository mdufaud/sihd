
#include <sihd/util/ThrowLogger.hpp>

namespace sihd::util
{

ThrowLogger::Exception::Exception(const LogInfo & info, std::string_view msg): _log_info(info), _msg(msg) {}

const char *ThrowLogger::Exception::what() const noexcept
{
    return _msg.data();
}

const LogInfo & ThrowLogger::Exception::log_info() const
{
    return _log_info;
}

ThrowLogger::ThrowLogger() {}

ThrowLogger::~ThrowLogger() {}

void ThrowLogger::log([[maybe_unused]] const LogInfo & info, std::string_view msg)
{
    throw Exception(info, msg);
}

} // namespace sihd::util
