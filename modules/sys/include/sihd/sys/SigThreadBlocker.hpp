#ifndef __SIHD_SYS_SIGTHREADBLOCKER_HPP__
#define __SIHD_SYS_SIGTHREADBLOCKER_HPP__

namespace sihd::sys
{

class SigThreadBlocker
{
    public:
        SigThreadBlocker(int sig);
        ~SigThreadBlocker();

        SigThreadBlocker(const SigThreadBlocker &) = delete;
        SigThreadBlocker & operator=(const SigThreadBlocker &) = delete;
        SigThreadBlocker(SigThreadBlocker &&) = delete;
        SigThreadBlocker & operator=(SigThreadBlocker &&) = delete;

        bool is_blocking() const { return _blocking; }
        int sig() const { return _sig; }

    private:
        int _sig;
        bool _blocking;
};

} // namespace sihd::sys

#endif
