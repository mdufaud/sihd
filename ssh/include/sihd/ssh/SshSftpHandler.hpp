#ifndef __SIHD_SSH_SSHSFTPHANDLER_HPP__
#define __SIHD_SSH_SSHSFTPHANDLER_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct sftp_session_struct;
struct sftp_client_message_struct;

namespace sihd::ssh
{

class SshSession;

/**
 * @brief SFTP (SSH File Transfer Protocol) subsystem handler.
 *
 * Handles SFTP subsystem requests from SSH clients. This implements a full
 * SFTP server that can handle file transfers, directory operations, and
 * file attribute queries.
 *
 * The handler uses callbacks to let you implement the actual filesystem
 * operations. This allows for flexible backends: real filesystem, virtual
 * filesystem, remote storage, etc.
 *
 * Supported operations:
 * - File operations: open, read, write, close, remove, rename
 * - Directory operations: opendir, readdir, mkdir, rmdir
 * - Attribute operations: stat, lstat, fstat, setstat
 * - Path operations: realpath, readlink, symlink
 *
 */
class SshSftpHandler: public ISshSubsystemHandler
{
    public:
        /**
         * @brief File attributes for SFTP responses.
         */
        struct SftpAttributes
        {
                std::string name;
                std::string longname; // ls -l style output
                uint32_t permissions;
                uint32_t uid;
                uint32_t gid;
                uint64_t size;
                uint64_t atime;
                uint64_t mtime;
                uint8_t type; // SSH_FILEXFER_TYPE_*

                static SftpAttributes from_stat(const std::string & name, const struct stat & st);
        };

        /**
         * @brief Result for stat/lstat operations.
         */
        struct StatResult
        {
                bool success;
                uint32_t error_code; // SSH_FX_*
                SftpAttributes attrs;

                static StatResult ok(const SftpAttributes & attrs);
                static StatResult error(uint32_t code);
        };

        /**
         * @brief Result for directory listing.
         */
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

        /**
         * @brief Result for file read operations.
         */
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

        /**
         * @brief Result for generic operations (write, mkdir, etc.).
         */
        struct OpResult
        {
                bool success;
                uint32_t error_code;
                std::string message;

                static OpResult ok();
                static OpResult error(uint32_t code, const std::string & msg = "");
        };

        SshSftpHandler();
        ~SshSftpHandler() override;

        /**
         * @brief Set the root path for default filesystem operations.
         *
         * When set, the default callbacks will operate on the real filesystem
         * under this root path. All client paths will be resolved relative to
         * this root, with path traversal protection.
         *
         * @param path Root directory path
         */
        void set_root_path(std::string_view path);

        // ISshSubsystemHandler interface
        bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const struct winsize & winsize) override;
        void on_eof() override;
        int on_close() override;

        int stdout_fd() const override { return -1; }
        int stderr_fd() const override { return -1; }
        SshChannel *channel() const override { return _channel; }
        bool is_running() const override { return _running; }

        /**
         * SFTP manages its own channel I/O via libssh's sftp_get_client_message.
         * This tells the SSH server not to intercept channel data.
         */
        bool manages_channel_io() const override { return true; }

    private:
        // Message handlers
        void handle_init();
        void handle_realpath(sftp_client_message_struct *msg);
        void handle_stat(sftp_client_message_struct *msg, bool follow_links);
        void handle_opendir(sftp_client_message_struct *msg);
        void handle_readdir(sftp_client_message_struct *msg);
        void handle_close(sftp_client_message_struct *msg);
        void handle_mkdir(sftp_client_message_struct *msg);
        void handle_rmdir(sftp_client_message_struct *msg);
        void handle_open(sftp_client_message_struct *msg);
        void handle_read(sftp_client_message_struct *msg);
        void handle_write(sftp_client_message_struct *msg);
        void handle_remove(sftp_client_message_struct *msg);
        void handle_rename(sftp_client_message_struct *msg);
        void handle_setstat(sftp_client_message_struct *msg);
        void handle_readlink(sftp_client_message_struct *msg);
        void handle_symlink(sftp_client_message_struct *msg);

        // Utility
        std::string resolve_path(const std::string & path);
        std::string generate_handle();
        void reply_status(sftp_client_message_struct *msg, uint32_t status, const std::string & message = "");

        // Deferred initialization - called on first on_data when SSH_FXP_INIT is available
        bool do_init();

        SshChannel *_channel;
        SshSession *_session;
        sftp_session_struct *_sftp;
        bool _running;
        bool _initialized; // True after sftp_server_init succeeds
        std::string _root_path;

        // Handle management (opaque handles -> paths)
        uint64_t _next_handle_id;
        std::map<std::string, std::string> _handle_to_path;
        std::map<std::string, int> _handle_to_fd;     // For file handles
        std::map<std::string, size_t> _dir_positions; // For directory enumeration
};

} // namespace sihd::ssh

#endif
