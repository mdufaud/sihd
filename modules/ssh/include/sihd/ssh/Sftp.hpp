#ifndef __SIHD_SSH_SFTP_HPP__
#define __SIHD_SSH_SFTP_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <sys/types.h> // mode_t, uid_t, gid_t
#else
typedef unsigned int gid_t;
#endif

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
        Sftp(void *session);
        ~Sftp();

        Sftp(const Sftp & other) = delete;
        Sftp & operator=(const Sftp &) = delete;

        sihd::util::Result<void> open();
        bool is_open() const;
        void close();

        sihd::util::Result<void> mkdir(std::string_view path, mode_t mode = 0755);
        sihd::util::Result<void> symlink(std::string_view from, std::string_view to);

        // TODO replace std::string return with optional
        std::string readlink(std::string_view path);

        sihd::util::Result<void>
            send_file(std::string_view local_path, std::string_view remote_path, mode_t mode = 0644);
        sihd::util::Result<void> get_file(std::string_view remote_path, std::string_view local_path);

        // TODO replace out param return with optional
        sihd::util::Result<void> list_dir(std::string_view path, std::vector<SftpAttribute> & list);
        // TODO replace out param return with optional
        sihd::util::Result<void> list_dir_filenames(std::string_view path, std::vector<std::string> & list);

        sihd::util::Result<void> rename(std::string_view from, std::string_view to);
        sihd::util::Result<void> chmod(std::string_view path, mode_t mode);
        sihd::util::Result<void> chown(std::string_view path, uid_t owner, gid_t group);

        sihd::util::Result<void> rmdir(std::string_view path);
        sihd::util::Result<void> rm(std::string_view path);

        std::vector<SftpExtension> extensions();

        int version();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::ssh

#endif
