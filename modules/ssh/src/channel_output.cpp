#include "channel_output.hpp"

#include <sihd/util/Logger.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

using namespace sihd::util;

namespace
{

void consume_written(std::string & unsent, int wrote, std::string_view stream)
{
    if (wrote < 0)
    {
        SIHD_LOG(error, "SshChannel: could not write {} to the channel", stream);
        unsent.clear();
    }
    else
        unsent.erase(0, static_cast<size_t>(wrote));
}

} // namespace

void write_unsent(SshChannel & channel, std::string & unsent)
{
    if (!unsent.empty())
        consume_written(unsent, channel.write(ArrCharView(unsent.data(), unsent.size())), "stdout");
}

void write_unsent_stderr(SshChannel & channel, std::string & unsent)
{
    if (!unsent.empty())
        consume_written(unsent, channel.write_stderr(ArrCharView(unsent.data(), unsent.size())), "stderr");
}

} // namespace sihd::ssh
