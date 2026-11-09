#ifndef __SIHD_SSH_CHANNEL_OUTPUT_HPP__
#define __SIHD_SSH_CHANNEL_OUTPUT_HPP__

#include <string>

#include <sihd/ssh/SshChannel.hpp>

namespace sihd::ssh
{

// the server session never blocks: what the client window refuses stays in unsent
void write_unsent(SshChannel & channel, std::string & unsent);
void write_unsent_stderr(SshChannel & channel, std::string & unsent);

} // namespace sihd::ssh

#endif
