#ifndef __SIHD_SSH_SSHSCP_HPP__
# define __SIHD_SSH_SSHSCP_HPP__

# include <string>

# include <libssh/libssh.h>

namespace sihd::ssh
{

class SshScp
{
    public:
        SshScp(ssh_session session);
        virtual ~SshScp();

        bool remote_opened();
        bool open_remote(std::string_view remote_path);
#pragma message("TODO - change to ArrCharView")
        bool push_file_content(std::string_view remote_path, const char *buf,
                                size_t size, int mode = 0644);
        bool push_file(std::string_view local_path, std::string_view remote_path,
                        size_t max_size = 0, int mode = 0644);
        bool push_dir(std::string_view name, int mode = 0755);
        bool leave_dir();

        bool pull_file(std::string_view remote_path, std::string_view local_path);

        void close();

        const char *get_warning();

        ssh_scp scp() const { return _ssh_scp_ptr; }
        ssh_session session() const { return _ssh_session_ptr; }

    protected:

    private:
        bool _open(int flags, std::string_view location);

        bool _remote_opened;
        ssh_scp _ssh_scp_ptr;
        ssh_session _ssh_session_ptr;
};

}

#endif