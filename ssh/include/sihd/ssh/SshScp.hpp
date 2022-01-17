#ifndef __SIHD_SSH_SSHSCP_HPP__
# define __SIHD_SSH_SSHSCP_HPP__

# include <libssh/libssh.h>
# include <string>

namespace sihd::ssh
{

class SshScp
{
    public:
        SshScp(ssh_session session);
        virtual ~SshScp();

        bool remote_opened();
        bool open_remote(const std::string & remote_path);
        bool push_file_content(const std::string & remote_path, const char *buf,
                                size_t size, int mode = 0644);
        bool push_file(const std::string & local_path, const std::string & remote_path,
                        size_t max_size = 0, int mode = 0644);
        bool push_dir(const std::string & name, int mode = 0755);
        bool leave_dir();

        bool pull_file(const std::string & remote_path, const std::string & local_path);

        void close();

        const char *get_warning();

        ssh_scp scp() const { return _ssh_scp_ptr; }
        ssh_session session() const { return _ssh_session_ptr; }

    protected:

    private:
        bool _open(int flags, const std::string & location);

        bool _remote_opened;
        ssh_scp _ssh_scp_ptr;
        ssh_session _ssh_session_ptr;
};

}

#endif