#ifndef __SIHD_SSH_SSHCOMMAND_HPP__
#define __SIHD_SSH_SSHCOMMAND_HPP__

#include <memory>
#include <string_view>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Waitable.hpp>

#include <sihd/ssh/SshChannel.hpp>

#pragma message("TODO pImpl")
struct ssh_channel_callbacks_struct;
struct ssh_channel_struct;
struct ssh_session_struct;

namespace sihd::ssh
{

class SshCommand
{
    public:
        SshCommand(ssh_session_struct *session);
        ~SshCommand();

        SshCommand(const SshCommand & other) = delete;
        SshCommand & operator=(const SshCommand &) = delete;

        bool execute(std::string_view cmd);
        bool execute_async(std::string_view cmd);
        bool input(sihd::util::ArrCharView view);
        bool wait(time_t timeout_nano = 0, time_t milliseconds_poll_time = 1);

        sihd::util::IHandler<std::string_view, bool> *output_handler;

        SshChannel & channel() { return _channel; }

        int exit_status();
        bool core_dumped();
        const std::string & exit_signal_str();
        const std::string & exit_signal_error();

    protected:
        void _callback_exit_status(int exit_status);
        void _callback_exit_signal(const char *signal, int core, const char *errmsg);
        void _callback_channel_output(char *buf, size_t size, bool is_stderr);

        static int ssh_command_channel_data_callback(ssh_session_struct *session,
                                                     ssh_channel_struct *channel,
                                                     void *data,
                                                     uint32_t len,
                                                     int is_stderr,
                                                     void *userdata);
        static void ssh_command_exit_signal_callback(ssh_session_struct *session,
                                                     ssh_channel_struct *channel,
                                                     const char *signal,
                                                     int core,
                                                     const char *errmsg,
                                                     const char *lang,
                                                     void *userdata);
        static void ssh_command_exit_status_callback(ssh_session_struct *session,
                                                     ssh_channel_struct *channel,
                                                     int exit_status,
                                                     void *userdata);

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
        std::unique_ptr<struct ssh_channel_callbacks_struct> _ssh_callbacks_ptr;

        SshChannel _channel;
        sihd::util::Waitable _waitable;

        ssh_session_struct *_ssh_session_ptr;
        bool _stop;
};

} // namespace sihd::ssh

#endif