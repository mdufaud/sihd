#ifndef __SIHD_SYS_CONSOLE_HPP__
#define __SIHD_SYS_CONSOLE_HPP__

#include <memory>

#include <sihd/util/Result.hpp>

namespace sihd::sys::console
{

struct WindowSize
{
        int cols = 0;
        int rows = 0;
};

sihd::util::Result<WindowSize> size(int fd);

// stdin carries user-typed input, even through the android terminal bridge
bool is_interactive();

// utf-8 console output code page on windows, nothing to do elsewhere
void set_output_utf8();

class RawMode
{
    public:
        explicit RawMode(int fd);
        ~RawMode();

        RawMode(const RawMode &) = delete;
        RawMode & operator=(const RawMode &) = delete;

        bool valid() const;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
};

class NoEcho
{
    public:
        explicit NoEcho(int fd);
        ~NoEcho();

        NoEcho(const NoEcho &) = delete;
        NoEcho & operator=(const NoEcho &) = delete;

        bool valid() const;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
};

sihd::util::Result<void> enable_vt(int fd);

} // namespace sihd::sys::console

#endif
