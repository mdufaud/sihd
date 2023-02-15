#include <csignal>

#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/os.hpp>

namespace sihd::util
{

SIHD_LOGGER;

SigWaiter::SigWaiter(int sig)
{
    if (sig == -1)
      sig = SIGINT;
    _sig = sig;
    os::add_signal_handler(sig, this);
    _waitable.infinite_wait();
}

SigWaiter::~SigWaiter()
{
    os::clear_signal_handler(_sig, this);
}

void    SigWaiter::handle(int sig)
{
    if (sig == _sig)
        _waitable.notify_all();
}

}
