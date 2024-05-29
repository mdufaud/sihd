#ifndef __SIHD_SSH_SSHSCP_HPP__
#define __SIHD_SSH_SSHSCP_HPP__

#include <string>
#include <string_view>

struct ssh_scp_struct;
struct ssh_session_struct;

namespace sihd::ssh
{

class SshScp
{
    public:
        SshScp(ssh_session_struct *session);
        virtual ~SshScp();

        SshScp(const SshScp & other) = delete;
        SshScp & operator=(const SshScp &) = delete;

        bool remote_opened();
        bool open_remote(std::string_view remote_path);
        bool push_file_content(std::string_view remote_path, std::string_view data, int mode = 0644);
        bool push_file(std::string_view local_path, std::string_view remote_path, size_t max_size = 0, int mode = 0644);
        bool push_dir(std::string_view name, int mode = 0755);
        bool leave_dir();

        bool pull_file(std::string_view remote_path, std::string_view local_path);

        void close();

        const char *get_warning();

        const ssh_scp_struct *scp() const { return _ssh_scp_ptr; }
        const ssh_session_struct *session() const { return _ssh_session_ptr; }

    protected:

    private:
        bool _open(int flags, std::string_view location);

        bool _remote_opened;
        ssh_scp_struct *_ssh_scp_ptr;
        ssh_session_struct *_ssh_session_ptr;
};

} // namespace sihd::ssh

#endif