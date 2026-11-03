#include "com.hpp"

#include <stdexcept>

namespace sihd::sys::internal
{

using namespace sihd::util;

Result<void> init_com(std::optional<windows_::ComInit> & com, windows_::ComInit::Threading threading)
{
    try
    {
        com.emplace(threading);
    }
    catch (const std::runtime_error & error)
    {
        return std::unexpected(Error(ErrorCode::io_error, "could not initialize com: {}", error.what()));
    }
    return {};
}

} // namespace sihd::sys::internal
