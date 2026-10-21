#ifndef __SIHD_SYS_WINDOWS_COMINIT_HPP__
#define __SIHD_SYS_WINDOWS_COMINIT_HPP__

#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# error "sihd/sys/windows headers are windows only"
#endif

namespace sihd::sys::windows_
{

class ComInit
{
    public:
        enum class Threading
        {
            apartment,
            multithreaded,
        };

        explicit ComInit(Threading threading = Threading::apartment);
        ~ComInit();

        ComInit(const ComInit &) = delete;
        ComInit & operator=(const ComInit &) = delete;
        ComInit(ComInit &&) = delete;
        ComInit & operator=(ComInit &&) = delete;

    private:
        bool _initialized = false;
};

} // namespace sihd::sys::windows_

#endif
