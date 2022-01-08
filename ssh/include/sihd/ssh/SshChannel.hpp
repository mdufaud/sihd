#ifndef __SIHD_SSH_SSHCHANNEL_HPP__
# define __SIHD_SSH_SSHCHANNEL_HPP__

# include <sihd/util/platform.hpp>
# include <libssh/libssh.h>
# include <string>

namespace sihd::ssh
{

class SshChannel
{
    public:
        SshChannel(ssh_channel channel = nullptr);
        virtual ~SshChannel();

        void set_channel(ssh_channel channel);
        void clear_channel();

        void set_blocking(bool active);

        bool open_session();
        bool open_agent();
        bool open_x11(const std::string & addr, int port);
        bool open_forward(const std::string & remotehost, int remoteport,
                            const std::string & sourcehost, int localport);
#if LIBSSH_VERSION_MINOR > 7
        /*
        bool open_reverse_forward(const std::string & remotehost, int remoteport,
                                    const std::string & sourcehost, int localport);
        */
        bool open_forward_unix(const std::string & remotepath,
                                const std::string & sourcehost, int localport);
#endif
        bool is_open();
        bool close();

        bool request_subsystem(const std::string & subsys);
        bool request_sftp();
        bool request_x11(const std::string & protocol, const std::string & cookie,
                            int screen_number = 0, bool single_connection = false);
        bool request_pty();
        bool change_pty_size(int cols, int rows);
        bool request_shell();
        bool request_exec(const std::string & cmd);

        bool cancel_forward(const std::string & addr, int port);
        bool set_env(const std::string & name, const std::string & value);

        int poll();
        int poll_stderr();
        int poll_timeout(int timeout_ms);
        int poll_timeout_stderr(int timeout_ms);

        int read(char *buffer, size_t size);
        int read_stderr(char *buffer, size_t size);
        int read_timeout(char *buffer, size_t size, int timeout_ms);
        int read_timeout_stderr(char *buffer, size_t size, int timeout_ms);
        int read_nonblock(char *buffer, size_t size);
        int read_nonblock_stderr(char *buffer, size_t size);

        int write(const char *buffer, size_t size);
        int write_stderr(const char *buffer, size_t size);

        bool send_eof();
        bool is_eof();
        
        // ABRT - ALRM - FPE - HUP - ILL - INT - KILL - PIPE - QUIT - SEGV - TERM - USR1 - USR2
        bool send_signal(const std::string & sig);

        int exit_status();

        ssh_channel channel() const { return _ssh_channel_ptr; }
        ssh_session session() const;

    protected:
    
    private:
        void _init_channel_if_none();

        ssh_channel _ssh_channel_ptr;
};

}

#endif