#ifndef __SIHD_SSH_SSHSHELL_HPP__
#define __SIHD_SSH_SSHSHELL_HPP__

#include <string>

#include <sihd/ssh/SshChannel.hpp>

struct ssh_session_struct;

namespace sihd::ssh
{

class SshShell
{
    public:
        SshShell(ssh_session_struct *session);
        ~SshShell();

        SshShell(const SshShell & other) = delete;
        SshShell & operator=(const SshShell &) = delete;

        bool open(bool x11 = false);
        void close();
        bool read_loop();

        SshChannel & channel() { return _channel; }

    protected:

    private:
        ssh_session_struct *_ssh_session_ptr;
        SshChannel _channel;
};

} // namespace sihd::ssh

#endif