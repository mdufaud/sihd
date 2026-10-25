#ifndef __SIHD_SSH_SSHCHANNEL_HPP__
#define __SIHD_SSH_SSHCHANNEL_HPP__

#include <memory>
#include <string>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

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
        sihd::util::Result<void> open_x11(sihd::util::ZStringView addr, int port);
        sihd::util::Result<void> open_forward(sihd::util::ZStringView remotehost,
                                              int remoteport,
                                              sihd::util::ZStringView sourcehost,
                                              int localport);
        sihd::util::Result<void>
            open_forward_unix(const std::string & remotepath, const std::string & sourcehost, int localport);

        bool is_open();
        sihd::util::Result<void> close();

        sihd::util::Result<void> request_subsystem(sihd::util::ZStringView subsys);
        sihd::util::Result<void> request_sftp();
        sihd::util::Result<void> request_x11(sihd::util::ZStringView protocol,
                                             sihd::util::ZStringView cookie,
                                             int screen_number = 0,
                                             bool single_connection = false);
        sihd::util::Result<void> request_pty();
        sihd::util::Result<void> change_pty_size(int cols, int rows);
        sihd::util::Result<void> request_shell();
        sihd::util::Result<void> request_exec(sihd::util::ZStringView cmd);

        sihd::util::Result<void> cancel_forward(sihd::util::ZStringView addr, int port);
        sihd::util::Result<void> set_env(sihd::util::ZStringView name, sihd::util::ZStringView value);

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

        sihd::util::Result<void> request_send_exit_status(int exit_status);
        sihd::util::Result<void> request_send_exit_signal(sihd::util::ZStringView signum,
                                                          bool core_dumped,
                                                          sihd::util::ZStringView errmsg,
                                                          sihd::util::ZStringView lang);

        // ABRT - ALRM - FPE - HUP - ILL - INT - KILL - PIPE - QUIT - SEGV - TERM - USR1 - USR2
        sihd::util::Result<void> send_signal(sihd::util::ZStringView sig);

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