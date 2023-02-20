#ifndef __SIHD_SSH_SSHCOMMAND_HPP__
#define __SIHD_SSH_SSHCOMMAND_HPP__

#include <string_view>

#include <libssh/callbacks.h>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Waitable.hpp>

#include <sihd/ssh/SshChannel.hpp>

namespace sihd::ssh
{

class SshCommand
{
    public:
        SshCommand(ssh_session session);
        virtual ~SshCommand();

        bool execute(std::string_view cmd);
        bool execute_async(std::string_view cmd);
        bool input(sihd::util::ArrCharView view);
        bool wait(time_t timeout_nano = 0, time_t milliseconds_poll_time = 1);

        sihd::util::IHandler<char *, size_t, bool> *output_handler;

        SshChannel & channel() { return _channel; }

        int exit_status();
        bool core_dumped();
        const std::string & exit_signal_str();
        const std::string & exit_signal_error();

    protected:
        void _callback_exit_status(int exit_status);
        void _callback_exit_signal(const char *signal, int core, const char *errmsg);
        void _callback_channel_output(char *buf, size_t size, bool is_stderr);

        static int ssh_command_channel_data_callback(ssh_session session,
                                                     ssh_channel channel,
                                                     void *data,
                                                     uint32_t len,
                                                     int is_stderr,
                                                     void *userdata);
        static void ssh_command_exit_signal_callback(ssh_session session,
                                                     ssh_channel channel,
                                                     const char *signal,
                                                     int core,
                                                     const char *errmsg,
                                                     const char *lang,
                                                     void *userdata);
        static void
            ssh_command_exit_status_callback(ssh_session session, ssh_channel channel, int exit_status, void *userdata);

    private:
        bool _execute(std::string_view cmd, bool async);
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

} // namespace sihd::ssh

#endif