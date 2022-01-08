#ifndef __SIHD_SSH_SFTP_HPP__
# define __SIHD_SSH_SFTP_HPP__

# include <libssh/libssh.h>
# include <libssh/sftp.h>
# include <string>
# include <string_view>

namespace sihd::ssh
{

class Sftp
{
    public:
        Sftp(ssh_session session);
        virtual ~Sftp();

        bool open();
        void close();

        bool mkdir(std::string_view path, mode_t mode = 0755);
        bool symlink(std::string_view from, std::string_view to);

        bool rename(std::string_view from, std::string_view to);
        bool chmod(std::string_view path, mode_t mode);
        bool chown(std::string_view path, uid_t owner, gid_t group);

        bool rmdir(std::string_view path);
        bool rm(std::string_view path);

        int version();

        const char *error();

    protected:
    
    private:
        ssh_session _ssh_session_ptr;
        sftp_session _sftp_session_ptr;
};

}

#endif