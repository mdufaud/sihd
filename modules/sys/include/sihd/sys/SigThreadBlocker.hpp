#ifndef __SIHD_SYS_SIGTHREADBLOCKER_HPP__
#define __SIHD_SYS_SIGTHREADBLOCKER_HPP__

#include <sihd/sys/platform.hpp>

#if !defined(__SIHD_WINDOWS__)

# include <signal.h>

# include <initializer_list>
# include <span>

namespace sihd::sys
{

class SigThreadBlocker
{
    public:
        explicit SigThreadBlocker(int sig);
        explicit SigThreadBlocker(std::span<const int> sigs);
        SigThreadBlocker(std::initializer_list<int> sigs);
        ~SigThreadBlocker();

        SigThreadBlocker(const SigThreadBlocker &) = delete;
        SigThreadBlocker & operator=(const SigThreadBlocker &) = delete;
        SigThreadBlocker(SigThreadBlocker &&) = delete;
        SigThreadBlocker & operator=(SigThreadBlocker &&) = delete;

        bool is_blocking() const { return _blocking; }

    private:
        void _block(std::span<const int> sigs);

        sigset_t _old_set;
        sigset_t _blocked_set;
        bool _blocking;
};

} // namespace sihd::sys

#endif

#endif
