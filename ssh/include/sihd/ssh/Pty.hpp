#ifndef __SIHD_SSH_PTY_HPP__
#define __SIHD_SSH_PTY_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::ssh
{

struct PtySize
{
        uint16_t cols;
        uint16_t rows;
        uint16_t xpixel;
        uint16_t ypixel;
};

/**
 * Abstract PTY (pseudo-terminal) interface.
 *
 * Platform implementations:
 * - POSIX (Linux/macOS): forkpty()
 * - Windows 10 1809+: ConPTY (CreatePseudoConsole)
 */
class Pty
{
    public:
        virtual ~Pty() = default;

        // Returns nullptr if PTY is not supported on this platform
        static std::unique_ptr<Pty> create();
        static bool is_supported();

        // Configuration (call before spawn)
        virtual void set_shell(std::string_view shell) = 0;
        virtual void set_args(std::vector<std::string> args) = 0;
        virtual void set_env(std::string_view name, std::string_view value) = 0;
        virtual void set_size(const PtySize & size) = 0;
        virtual void set_working_directory(std::string_view path) = 0;

        // Lifecycle
        virtual bool spawn() = 0;
        virtual bool is_running() const = 0;
        // Blocking wait, returns exit code (-1 if not spawned)
        virtual int wait() = 0;
        virtual void terminate() = 0;

        // I/O
        // Non-blocking FD for poll/select (-1 if not spawned)
        virtual int read_fd() const = 0;
        // Non-blocking, returns 0 if no data, -1 on error
        virtual ssize_t read(void *buffer, size_t size) = 0;
        virtual ssize_t write(const void *data, size_t size) = 0;
        virtual bool resize(const PtySize & size) = 0;
        virtual void send_eof() = 0;
};

} // namespace sihd::ssh

#endif
