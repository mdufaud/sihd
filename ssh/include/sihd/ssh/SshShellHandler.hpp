#ifndef __SIHD_SSH_SSHSHELLHANDLER_HPP__
#define __SIHD_SSH_SSHSHELLHANDLER_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace sihd::ssh
{

/**
 * Shell session handler.
 *
 * Provides a simple shell implementation with customizable behavior:
 * - Welcome/goodbye messages
 * - Echo mode (default) or custom data callback
 * - EOF handling (Ctrl-D)
 *
 * For a real shell with fork/PTY, use with ForkSshCommandHandler
 * or set a custom data callback.
 *
 * Usage:
 *   auto handler = new SshShellHandler();
 *   handler->set_welcome_message("Welcome!\r\n");
 *   handler->set_data_callback([](SshChannel *ch, const void *data, size_t len) {
 *       // Process data...
 *       ch->write(...);
 *       return len;
 *   });
 */
class SshShellHandler: public ISshSubsystemHandler
{
    public:
        /**
         * Callback for processing incoming data.
         * @param channel The SSH channel
         * @param data Incoming data
         * @param len Data length
         * @return Bytes consumed, -1 on error
         */
        using DataCallback = std::function<int(SshChannel *channel, const void *data, size_t len)>;

        /**
         * Callback for resize events.
         */
        using ResizeCallback = std::function<void(const struct winsize & winsize)>;

        SshShellHandler();
        ~SshShellHandler() override;

        // Configuration (call before on_start)
        void set_welcome_message(std::string_view message);
        void set_goodbye_message(std::string_view message);
        void set_eof_byte(uint8_t byte);
        void set_handle_eof(bool enable);
        void set_data_callback(DataCallback callback);
        void set_resize_callback(ResizeCallback callback);

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

        // Accessors
        bool has_pty() const { return _has_pty; }
        const struct winsize & winsize() const { return _winsize; }

    private:
        SshChannel *_channel;
        bool _running;
        bool _has_pty;
        struct winsize _winsize;

        std::string _welcome_message;
        std::string _goodbye_message;
        uint8_t _eof_byte;
        bool _handle_eof;

        DataCallback _data_callback;
        ResizeCallback _resize_callback;
};

} // namespace sihd::ssh

#endif
