#ifndef __SIHD_SSH_SFTP_HPP__
# define __SIHD_SSH_SFTP_HPP__

# include <libssh/libssh.h>
# include <libssh/sftp.h>
# include <vector>
# include <string>
# include <string_view>
# include <memory>

namespace sihd::ssh
{

struct SftpAttributesDeleter
{
    void operator()(sftp_attributes_struct *ptr)
    {
        if (ptr != nullptr)
            sftp_attributes_free(ptr);
    }
};

using SftpAttributes = std::unique_ptr<sftp_attributes_struct, SftpAttributesDeleter>;

struct SftpFileDeleter
{
    void operator()(sftp_file_struct *ptr)
    {
        //TODO log error if ret != SSH_NO_ERROR
        if (ptr != nullptr)
            sftp_close(ptr);
    }
};

using SftpFile = std::unique_ptr<sftp_file_struct, SftpFileDeleter>;

struct SftpDirDeleter
{
    void operator()(sftp_dir_struct *ptr)
    {
        //TODO log error if ret != SSH_NO_ERROR
        if (ptr != nullptr)
            sftp_closedir(ptr);
    }
};

using SftpDir = std::unique_ptr<sftp_dir_struct, SftpDirDeleter>;

class Sftp
{
    public:
        Sftp(ssh_session session);
        virtual ~Sftp();

        bool open();
        void close();

        bool mkdir(std::string_view path, mode_t mode = 0755);
        bool symlink(std::string_view from, std::string_view to);

        bool send_file(std::string_view local_path, std::string_view remote_path, mode_t mode = 0644);
        bool get_file(std::string_view remote_path, std::string_view local_path);

        bool list_dir(std::string_view path, std::vector<SftpAttributes> & list);
        bool list_dir_filenames(std::string_view path, std::vector<std::string> & list);

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