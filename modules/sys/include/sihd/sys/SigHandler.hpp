#ifndef __SIHD_SYS_SIGHANDLER_HPP__
#define __SIHD_SYS_SIGHANDLER_HPP__

#include <signal.h>

#include <sihd/util/Result.hpp>

namespace sihd::sys
{

class SigHandler
{
    public:
        SigHandler();
        SigHandler(int sig);
        ~SigHandler();

        sihd::util::Result<void> handle(int sig);
        sihd::util::Result<void> unhandle();

        bool call_previous_handler() const;

        bool is_handling() const;
        int sig() const { return _sig; }

    protected:

    private:
        using sig_handler = void (*)(int);

        int _sig;
        sig_handler _previous_handler;
};

} // namespace sihd::sys

#endif
