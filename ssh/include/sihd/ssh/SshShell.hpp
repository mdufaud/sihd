#ifndef __SIHD_SSH_SSHSHELL_HPP__
#define __SIHD_SSH_SSHSHELL_HPP__

#include <memory>
#include <string>

#include <sihd/ssh/SshChannel.hpp>

namespace sihd::ssh
{

class SshShell
{
    public:
        SshShell(void *session);
        ~SshShell();

        SshShell(const SshShell & other) = delete;
        SshShell & operator=(const SshShell &) = delete;

        bool open(bool x11 = false);
        void close();
        bool read_loop();

        SshChannel & channel();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::ssh

#endif