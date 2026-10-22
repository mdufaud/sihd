#ifndef __SIHD_SSH_SFTP_HPP__
#define __SIHD_SSH_SFTP_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

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

        sihd::util::Result<void> mkdir(const std::string & path, mode_t mode = 0755);
        sihd::util::Result<void> symlink(const std::string & from, const std::string & to);

        // TODO replace std::string return with optional
        std::string readlink(const std::string & path);

        sihd::util::Result<void>
            send_file(const std::string & local_path, const std::string & remote_path, mode_t mode = 0644);
        sihd::util::Result<void> get_file(const std::string & remote_path, const std::string & local_path);

        // TODO replace out param return with optional
        sihd::util::Result<void> list_dir(const std::string & path, std::vector<SftpAttribute> & list);
        // TODO replace out param return with optional
        sihd::util::Result<void> list_dir_filenames(const std::string & path, std::vector<std::string> & list);

        sihd::util::Result<void> rename(const std::string & from, const std::string & to);
        sihd::util::Result<void> chmod(const std::string & path, mode_t mode);
        sihd::util::Result<void> chown(const std::string & path, uid_t owner, gid_t group);

        sihd::util::Result<void> rmdir(const std::string & path);
        sihd::util::Result<void> rm(const std::string & path);

        std::vector<SftpExtension> extensions();

        int version();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::ssh

#endif
