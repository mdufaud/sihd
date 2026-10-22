#include <sihd/ssh/ISshServerHandler.hpp>

namespace sihd::ssh
{

void ISshServerHandler::on_poll([[maybe_unused]] SshServer *server) {}

bool ISshServerHandler::channel_bypasses_data_callback([[maybe_unused]] SshChannel *channel) const
{
    return false;
}

} // namespace sihd::ssh
