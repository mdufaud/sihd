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
                // crash dump file: minidump on windows, text report on unix; empty keeps stderr only
                std::string dump_path;
                // unix: re-raise with default disposition (core dump kept); windows: chain to WER/JIT, false terminates
                bool rethrow = true;
        };

        CrashHandler() = default;
        explicit CrashHandler(const Options & options);

        ~CrashHandler();

        CrashHandler(const CrashHandler &) = delete;
        CrashHandler & operator=(const CrashHandler &) = delete;

        // false when a handler is already installed, by this instance or another
        bool install(const Options & options);
        void uninstall();
        bool is_installed() const { return _installed; }

    private:
        struct Impl;
        Impl *_impl = nullptr;
        Options _options;
        bool _installed = false;
};

} // namespace sihd::sys

#endif
