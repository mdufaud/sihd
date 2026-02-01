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

#if defined(__SIHD_WINDOWS__)
    // On Windows, we must use std::signal which installs and returns previous handler
    // We temporarily install SIG_DFL just to get the previous handler, then immediately
    // install our custom handler via signal::handle()
    _previous_handler = std::signal(sig, SIG_DFL);
    if (_previous_handler == SIG_ERR)
    {
        SIHD_LOG(warning, "SigHandler: could not get previous handler for signal '{}'", _sig);
    }
#else
    // On POSIX, use sigaction to query previous handler without installing anything
    struct sigaction old_sa;
    if (sigaction(sig, nullptr, &old_sa) == 0)
    {
        _previous_handler = old_sa.sa_handler;
    }
    else
    {
        SIHD_LOG(warning, "SigHandler: could not get previous handler for signal '{}'", _sig);
        _previous_handler = SIG_ERR;
    }
#endif

    return signal::handle(sig);
}

bool SigHandler::unhandle()
{
    bool ret = true;

    if (this->is_handling())
    {
        if (std::signal(_sig, _previous_handler) == SIG_ERR)
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
