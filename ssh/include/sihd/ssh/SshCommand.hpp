#ifndef __SIHD_SSH_SSHCOMMAND_HPP__
# define __SIHD_SSH_SSHCOMMAND_HPP__

# include <sihd/ssh/SshChannel.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/Waitable.hpp>
# include <libssh/callbacks.h>
# include <string_view>

namespace sihd::ssh
{

class SshCommand
{
    public:
        SshCommand(ssh_session session);
        virtual ~SshCommand();

        bool execute(const std::string & cmd, bool async = false);
        bool input(const char *buf, size_t size);
        bool wait(time_t timeout_nano = 0);

        void _callback_exit_status(int exit_status);
        void _callback_exit_signal(const char *signal, int core, const char *errmsg);
        void _callback_channel_output(char *buf, size_t size, bool is_stderr);

        sihd::util::IHandler<char *, size_t, bool> *output_handler;

        SshChannel & channel() { return _channel; }

        int exit_status();
        bool core_dumped();
        const std::string & exit_signal_str();
        const std::string & exit_signal_error();

    protected:
    
    private:
        void _reset_command_status();

        struct CommandStatus
        {
            int exit_status;
            bool core_dumped;
            std::string signal_str;
            std::string err_msg;
        };
        CommandStatus _command_status;
        struct ssh_channel_callbacks_struct _ssh_callbacks;

        SshChannel _channel;
        sihd::util::Waitable _waitable;

        ssh_session _ssh_session_ptr;
        bool _stop;
};

}

#endif