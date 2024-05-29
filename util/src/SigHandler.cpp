#include <csignal>

#include <sihd/util/Logger.hpp>
#include <sihd/util/SigHandler.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/signal.hpp>

#if defined(__SIHD_WINDOWS__)
typedef void (*sighandler_t)(int);
#endif

namespace sihd::util
{

SIHD_LOGGER;

SigHandler::SigHandler(): _sig(-1), _previous_handler(SIG_ERR) {}

SigHandler::SigHandler(int sig): SigHandler()
{
    this->handle(sig);
}

SigHandler::~SigHandler()
{
    this->unhandle();
}

bool SigHandler::handle(int sig)
{
    this->unhandle();

    _sig = sig;
    _previous_handler = ::signal(sig, SIG_DFL);

    if (_previous_handler == SIG_ERR)
    {
        SIHD_LOG(warning, "SigHandler: could not get previous handler for signal '{}'", _sig);
    }

    return signal::handle(sig);
}

bool SigHandler::unhandle()
{
    bool ret = true;

    if (this->is_handling())
    {
        if (::signal(_sig, _previous_handler) == SIG_ERR)
        {
            SIHD_LOG(error, "SigHandler: could not unhandle signal {}", _sig);
            ret = false;
        }
    }

    _previous_handler = SIG_ERR;
    _sig = -1;

    return ret;
}

bool SigHandler::is_handling() const
{
    return _sig >= 0 && _previous_handler != SIG_ERR;
}

bool SigHandler::call_previous_handler() const
{
    const bool can_call = this->is_handling() && _previous_handler != SIG_DFL;
    if (can_call)
        _previous_handler(_sig);
    return can_call;
}

} // namespace sihd::util
