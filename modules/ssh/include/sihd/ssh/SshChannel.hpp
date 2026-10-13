#ifndef __SIHD_SSH_SSHCHANNEL_HPP__
#define __SIHD_SSH_SSHCHANNEL_HPP__

#include <memory>
#include <string_view>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::ssh
{

class SshChannel
{
    public:
        SshChannel(void *channel = nullptr);
        ~SshChannel();

        void set_channel(void *channel);
        void clear_channel();

        void set_blocking(bool active);

        sihd::util::Result<void> open_session();
        sihd::util::Result<void> open_agent();
        sihd::util::Result<void> open_x11(std::string_view addr, int port);
        sihd::util::Result<void>
            open_forward(std::string_view remotehost, int remoteport, std::string_view sourcehost, int localport);
        sihd::util::Result<void>
            open_forward_unix(std::string_view remotepath, std::string_view sourcehost, int localport);

        bool is_open();
        sihd::util::Result<void> close();

        sihd::util::Result<void> request_subsystem(std::string_view subsys);
        sihd::util::Result<void> request_sftp();
        sihd::util::Result<void> request_x11(std::string_view protocol,
                                             std::string_view cookie,
                                             int screen_number = 0,
                                             bool single_connection = false);
        sihd::util::Result<void> request_pty();
        sihd::util::Result<void> change_pty_size(int cols, int rows);
        sihd::util::Result<void> request_shell();
        sihd::util::Result<void> request_exec(std::string_view cmd);

        sihd::util::Result<void> cancel_forward(std::string_view addr, int port);
        sihd::util::Result<void> set_env(std::string_view name, std::string_view value);

        int poll();
        int poll_stderr();
        int poll_timeout(int timeout_ms);
        int poll_timeout_stderr(int timeout_ms);

        int read(sihd::util::IArray & array);
        int read_stderr(sihd::util::IArray & array);
        int read_timeout(sihd::util::IArray & array, int timeout_ms);
        int read_timeout_stderr(sihd::util::IArray & array, int timeout_ms);
        int read_nonblock(sihd::util::IArray & array);
        int read_nonblock_stderr(sihd::util::IArray & array);

        int write(sihd::util::ArrCharView view);
        int write_stderr(sihd::util::ArrCharView view);

        sihd::util::Result<void> send_eof();
        bool is_eof();

        // Server-side methods
        sihd::util::Result<void> request_send_exit_status(int exit_status);
        sihd::util::Result<void> request_send_exit_signal(std::string_view signum,
                                                          bool core_dumped,
                                                          std::string_view errmsg,
                                                          std::string_view lang);

        // ABRT - ALRM - FPE - HUP - ILL - INT - KILL - PIPE - QUIT - SEGV - TERM - USR1 - USR2
        sihd::util::Result<void> send_signal(std::string_view sig);

        int exit_status();

        void *channel() const;
        void *session() const;

        void set_userdata(void *userdata);
        void *userdata() const;

        // Detach pointer without freeing (use when libssh already freed the channel)
        void detach();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::ssh

#endif