#ifndef __SIHD_SSH_SFTP_HPP__
#define __SIHD_SSH_SFTP_HPP__

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#pragma message("TODO pImpl")
struct sftp_file_struct;
struct sftp_attributes_struct;
struct ssh_session_struct;
struct sftp_session_struct;

namespace sihd::ssh
{

struct SftpExtension
{
        std::string name;
        std::string data; // version most of the time
};

class SftpAttribute
{
    public:
        SftpAttribute(std::string_view name, uint8_t type, size_t size);
        ~SftpAttribute();

        const std::string & name() const;
        bool is_dir() const;
        bool is_file() const;
        bool is_link() const;
        uint8_t type() const;
        uint64_t size() const;

    private:
        std::string _name;
        uint8_t _type;
        size_t _size;
};

class Sftp
{
    public:
        Sftp(ssh_session_struct *session);
        ~Sftp();

        Sftp(const Sftp & other) = delete;
        Sftp & operator=(const Sftp &) = delete;

        bool open();
        bool is_open() const;
        void close();

        bool mkdir(std::string_view path, mode_t mode = 0755);
        bool symlink(std::string_view from, std::string_view to);

        // TODO replace std::string return with optional
        std::string readlink(std::string_view path);

        bool send_file(std::string_view local_path, std::string_view remote_path, mode_t mode = 0644);
        bool get_file(std::string_view remote_path, std::string_view local_path);

        // TODO replace bool return with optional
        bool list_dir(std::string_view path, std::vector<SftpAttribute> & list);
        // TODO replace bool return with optional
        bool list_dir_filenames(std::string_view path, std::vector<std::string> & list);

        bool rename(std::string_view from, std::string_view to);
        bool chmod(std::string_view path, mode_t mode);
        bool chown(std::string_view path, uid_t owner, gid_t group);

        bool rmdir(std::string_view path);
        bool rm(std::string_view path);

        std::vector<SftpExtension> extensions();

        int version();

        const char *error();

    protected:

    private:
        ssh_session_struct *_ssh_session_ptr;
        sftp_session_struct *_sftp_session_ptr;
};

} // namespace sihd::ssh

#endif