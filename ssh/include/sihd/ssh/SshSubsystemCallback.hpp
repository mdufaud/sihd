#ifndef __SIHD_SSH_SSHSUBSYSTEMCALLBACK_HPP__
#define __SIHD_SSH_SSHSUBSYSTEMCALLBACK_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <functional>
#include <string>

namespace sihd::ssh
{

/**
 * SSH callback subsystem - request/response pattern.
 *
 * Receives a command string, calls the user callback, sends output back.
 * The entire exchange is synchronous in on_start().
 * Without a callback, the request is rejected.
 */
class SshSubsystemCallback: public ISshSubsystemHandler
{
    public:
        struct Result
        {
                std::string output;
                std::string error_output;
                int exit_code;
        };

        using Callback = std::function<Result(const std::string & command)>;

        explicit SshSubsystemCallback(std::string_view command);
        ~SshSubsystemCallback() override;

        // Must be set before on_start()
        void set_callback(Callback callback);

        const std::string & command() const { return _command; }

        // ISshSubsystemHandler
        bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const struct winsize & winsize) override;
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
