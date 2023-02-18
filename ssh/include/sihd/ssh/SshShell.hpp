#ifndef __SIHD_SSH_SSHSHELL_HPP__
# define __SIHD_SSH_SSHSHELL_HPP__

# include <libssh/libssh.h>

# include <string>

# include <sihd/ssh/SshChannel.hpp>

namespace sihd::ssh
{

class SshShell
{
    public:
        SshShell(ssh_session session);
        virtual ~SshShell();

        bool open(bool x11 = false);
        void close();
        bool read_loop();

        SshChannel & channel() { return _channel; }

    protected:

    private:
        ssh_session _ssh_session_ptr;
        SshChannel _channel;

};

}

#endif