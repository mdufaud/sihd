#ifndef __SIHD_SYS_SIGHANDLER_HPP__
#define __SIHD_SYS_SIGHANDLER_HPP__

#include <signal.h>

#include <expected>

#include <sihd/util/Error.hpp>

namespace sihd::sys
{

class SigHandler
{
    public:
        SigHandler();
        SigHandler(int sig);
        ~SigHandler();

        std::expected<void, sihd::util::Error> handle(int sig);
        std::expected<void, sihd::util::Error> unhandle();

        bool call_previous_handler() const;

        bool is_handling() const;
        int sig() const { return _sig; }

    protected:

    private:
        typedef void (*sig_handler)(int);

        int _sig;
        sig_handler _previous_handler;
};

} // namespace sihd::sys

#endif
