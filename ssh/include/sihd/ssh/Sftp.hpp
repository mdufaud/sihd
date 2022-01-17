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

class SftpAttribute
{
    public:
        SftpAttribute(sftp_attributes ptr);
        SftpAttribute(SftpAttribute && sftp_attr);
        ~SftpAttribute();

        const char *name() const;
        bool is_dir() const;
        bool is_file() const;
        bool is_link() const;
        uint64_t size() const;

        sftp_attributes attr;
};

class Sftp
{
    public:
        Sftp(ssh_session session);
        virtual ~Sftp();

        bool open();
        void close();

        bool mkdir(const std::string & path, mode_t mode = 0755);
        bool symlink(const std::string & from, const std::string & to);

        bool send_file(const std::string & local_path, const std::string & remote_path, mode_t mode = 0644);
        bool get_file(const std::string & remote_path, const std::string & local_path);

        bool list_dir(const std::string & path, std::vector<SftpAttribute> & list);
        bool list_dir_filenames(const std::string & path, std::vector<std::string> & list);

        bool rename(const std::string & from, const std::string & to);
        bool chmod(const std::string & path, mode_t mode);
        bool chown(const std::string & path, uid_t owner, gid_t group);

        bool rmdir(const std::string & path);
        bool rm(const std::string & path);

        int version();

        const char *error();

    protected:

    private:
        ssh_session _ssh_session_ptr;
        sftp_session _sftp_session_ptr;
};

}

#endif