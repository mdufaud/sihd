#ifndef __SIHD_SSH_SSHSUBSYSTEMSFTP_HPP__
#define __SIHD_SSH_SSHSUBSYSTEMSFTP_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sihd::ssh
{

class SshSession;

/**
 * SFTP subsystem handler.
 * Operates on real filesystem under set_root_path() with path traversal protection.
 */
class SshSubsystemSftp: public ISshSubsystemHandler
{
    public:
        struct SftpAttributes
        {
                std::string name;
                std::string longname; // ls -l format
                uint32_t permissions;
                uint32_t uid;
                uint32_t gid;
                uint64_t size;
                uint64_t atime;
                uint64_t mtime;
                uint8_t type; // SSH_FILEXFER_TYPE_*

                static SftpAttributes from_stat(const std::string & name, const struct stat & st);
        };

        struct StatResult
        {
                bool success;
                uint32_t error_code; // SSH_FX_*
                SftpAttributes attrs;

                static StatResult ok(const SftpAttributes & attrs);
                static StatResult error(uint32_t code);
        };

        struct ReaddirResult
        {
                bool success;
                bool eof; // No more entries
                uint32_t error_code;
                std::vector<SftpAttributes> entries;

                static ReaddirResult ok(std::vector<SftpAttributes> entries, bool eof = false);
                static ReaddirResult end();
                static ReaddirResult error(uint32_t code);
        };

        struct ReadResult
        {
                bool success;
                bool eof;
                uint32_t error_code;
                std::vector<char> data;

                static ReadResult ok(std::vector<char> data);
                static ReadResult end();
                static ReadResult error(uint32_t code);
        };

        struct OpResult
        {
                bool success;
                uint32_t error_code;
                std::string message;

                static OpResult ok();
                static OpResult error(uint32_t code, const std::string & msg = "");
        };

        SshSubsystemSftp();
        ~SshSubsystemSftp() override;

        void set_root_path(std::string_view path);

        bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const struct winsize & winsize) override;
        void on_eof() override;
        int on_close() override;

        int stdout_fd() const override { return -1; }
        int stderr_fd() const override { return -1; }
        SshChannel *channel() const override;
        bool is_running() const override;

        // SFTP manages its own channel I/O via sftp_get_client_message
        bool manages_channel_io() const override { return true; }

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::ssh

#endif
