#include <sihd/ssh/ISshSubsystemHandler.hpp>

namespace sihd::ssh
{

bool ISshSubsystemHandler::forward_output()
{
    return false;
}

bool ISshSubsystemHandler::manages_channel_io() const
{
    return false;
}

} // namespace sihd::ssh
