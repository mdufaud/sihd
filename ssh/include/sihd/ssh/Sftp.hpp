#ifndef __SIHD_SSH_SFTP_HPP__
#define __SIHD_SSH_SFTP_HPP__

#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct sftp_file_struct;
struct sftp_attributes_struct;
struct ssh_session_struct;
struct sftp_session_struct;

namespace sihd::ssh
{

class SftpAttribute
{
    public:
        SftpAttribute(sftp_attributes_struct *ptr);
        SftpAttribute(SftpAttribute && sftp_attr);
        ~SftpAttribute();

        SftpAttribute(const SftpAttribute & other) = delete;
        SftpAttribute & operator=(const SftpAttribute &) = delete;

        const char *name() const;
        bool is_dir() const;
        bool is_file() const;
        bool is_link() const;
        uint64_t size() const;

        sftp_attributes_struct *attr;
};

class Sftp
{
    public:
        Sftp(ssh_session_struct *session);
        ~Sftp();

        Sftp(const Sftp & other) = delete;
        Sftp & operator=(const Sftp &) = delete;

        bool open();
        void close();

        bool mkdir(std::string_view path, mode_t mode = 0755);
        bool symlink(std::string_view from, std::string_view to);

        bool send_file(std::string_view local_path, std::string_view remote_path, mode_t mode = 0644);
        bool get_file(std::string_view remote_path, std::string_view local_path);

        bool list_dir(std::string_view path, std::vector<SftpAttribute> & list);
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
        ssh_session_struct *_ssh_session_ptr;
        sftp_session_struct *_sftp_session_ptr;
};

} // namespace sihd::ssh

#endif