#ifndef __SIHD_SSH_SSHSUBSYSTEMPTY_HPP__
#define __SIHD_SSH_SSHSUBSYSTEMPTY_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>
#include <sihd/ssh/Pty.hpp>

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
        bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const struct winsize & winsize) override;
        void on_eof() override;
        int on_close() override;

        int stdout_fd() const override;
        int stderr_fd() const override { return -1; }
        SshChannel *channel() const override { return _channel; }
        bool is_running() const override;

    private:
        SshChannel *_channel;
        std::unique_ptr<Pty> _pty;

        std::string _shell;
        std::vector<std::string> _args;
        std::vector<std::pair<std::string, std::string>> _env;
        std::string _working_dir;
};

} // namespace sihd::ssh

#endif
