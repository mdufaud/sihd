#ifndef __SIHD_SSH_SSHSUBSYSTEMPTY_HPP__
#define __SIHD_SSH_SSHSUBSYSTEMPTY_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>
#include <sihd/sys/Pty.hpp>
#include <sihd/util/Array.hpp>

#include <memory>
#include <string>
#include <vector>

namespace sihd::ssh
{

/**
 * SSH PTY handler - spawns a shell in a pseudo-terminal bridged to an SSH channel.
 *
 * Data flow: [SSH Client] <-> [SshChannel] <-> [SshSubsystemPty] <-> [Pty] <-> [Shell]
 *
 * stdout_fd() returns the PTY read descriptor for polling by the server.
 */
class SshSubsystemPty: public ISshSubsystemHandler
{
    public:
        SshSubsystemPty();
        ~SshSubsystemPty() override;

        static bool is_supported();

        // Configuration (call before on_start)
        void set_shell(std::string_view shell);
        void set_args(std::vector<std::string> args);
        void set_env(std::string_view name, std::string_view value);
        void set_working_directory(std::string_view path);

        // ISshSubsystemHandler
        bool on_start(SshChannel *channel, bool has_pty, const WinSize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const WinSize & winsize) override;
        void on_eof() override;
        int on_close() override;

        int stdout_fd() const override;
        int stderr_fd() const override { return -1; }
        SshChannel *channel() const override { return _channel; }
        bool is_running() const override;
        bool forward_output() override;

    private:
        void drain_pending_input();

        SshChannel *_channel;
        std::unique_ptr<sys::Pty> _pty;

        std::string _shell;
        std::vector<std::string> _args;
        std::vector<std::pair<std::string, std::string>> _env;
        std::string _working_dir;

        // Buffered client input not yet accepted by the PTY (partial write)
        std::string _pending_in;
        // Reused read buffer for forward_output (avoids per-tick alloc)
        sihd::util::ArrChar _read_buf;
};

} // namespace sihd::ssh

#endif
