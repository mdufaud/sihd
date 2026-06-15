#ifndef __SIHD_SSH_SSHCOMMAND_HPP__
#define __SIHD_SSH_SSHCOMMAND_HPP__

#include <memory>
#include <string_view>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/time.hpp>

namespace sihd::ssh
{

class SshCommand
{
    public:
        SshCommand(void *session);
        ~SshCommand();

        SshCommand(const SshCommand & other) = delete;
        SshCommand & operator=(const SshCommand &) = delete;

        bool execute(std::string_view cmd);
        bool execute_async(std::string_view cmd);
        bool input(sihd::util::ArrCharView view);
        bool wait(sihd::util::Duration timeout_nano = 0, sihd::util::time::UnixTime milliseconds_poll_time = 1);

        sihd::util::IHandler<std::string_view, bool> *output_handler;

        SshChannel & channel();

        int exit_status();
        bool core_dumped();
        const std::string & exit_signal_str();
        const std::string & exit_signal_error();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::ssh

#endif