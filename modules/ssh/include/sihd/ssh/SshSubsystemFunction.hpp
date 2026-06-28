#ifndef __SIHD_SSH_SSHSUBSYSTEMFUNCTION_HPP__
#define __SIHD_SSH_SSHSUBSYSTEMFUNCTION_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <functional>
#include <string>

namespace sihd::ssh
{

/**
 * SSH function subsystem - in-process request/response handler.
 *
 * Receives a command string, calls a user std::function, sends its output back.
 * The whole exchange runs synchronously in on_start(); no OS process is spawned.
 * Without a callback set, the request is rejected.
 *
 * Difference from SshSubsystemExec: Exec spawns a real OS process (sys::Process)
 * and streams its stdout/stderr; Function runs user C++ code and returns a Result.
 */
class SshSubsystemFunction: public ISshSubsystemHandler
{
    public:
        struct Result
        {
                std::string output;
                std::string error_output;
                int exit_code;
        };

        using Callback = std::function<Result(const std::string & command)>;

        explicit SshSubsystemFunction(std::string_view command);
        ~SshSubsystemFunction() override;

        // Must be set before on_start()
        void set_callback(Callback callback);

        const std::string & command() const { return _command; }

        // ISshSubsystemHandler
        bool on_start(SshChannel *channel, bool has_pty, const WinSize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const WinSize & winsize) override;
        void on_eof() override;
        int on_close() override;

        int stdout_fd() const override { return -1; }
        int stderr_fd() const override { return -1; }
        SshChannel *channel() const override { return _channel; }
        bool is_running() const override { return false; }

    private:
        SshChannel *_channel;
        std::string _command;
        int _exit_code;
        Callback _callback;
};

} // namespace sihd::ssh

#endif
