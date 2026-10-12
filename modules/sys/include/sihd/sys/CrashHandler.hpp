#ifndef __SIHD_SYS_CRASHHANDLER_HPP__
#define __SIHD_SYS_CRASHHANDLER_HPP__

#include <string>

namespace sihd::sys
{

class CrashHandler
{
    public:
        struct Options
        {
                // unix file descriptor the crash context and backtrace are written to
                int fd = 2;
                // windows minidump path; empty disables the dump
                std::string minidump_path;
                // unix: re-raise with the default disposition, preserving the core dump;
                // windows: chain to WER/JIT — false terminates with the exception code
                bool rethrow = true;
        };

        CrashHandler() = default;
        explicit CrashHandler(const Options & options);

        ~CrashHandler();

        // false when a handler is already installed, by this instance or another
        bool install(const Options & options);
        void uninstall();
        bool is_installed() const { return _installed; }

    private:
        Options _options;
        bool _installed = false;
};

} // namespace sihd::sys

#endif
