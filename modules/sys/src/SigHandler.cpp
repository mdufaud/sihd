#include <csignal>

#include <sihd/sys/SigHandler.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

#if defined(__SIHD_WINDOWS__)
typedef void (*sighandler_t)(int);
#endif

using namespace sihd::util;
namespace sihd::sys
{

SIHD_LOGGER;

SigHandler::SigHandler(): _sig(-1), _previous_handler(SIG_ERR) {}

SigHandler::SigHandler(int sig): SigHandler()
{
    SIHD_UNEXPECTED_LOG(this->handle(sig));
}

SigHandler::~SigHandler()
{
    (void)this->unhandle();
}

std::expected<void, Error> SigHandler::handle(int sig)
{
    auto unhandled = this->unhandle();
    if (!unhandled)
        return unhandled;

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

std::expected<void, Error> SigHandler::unhandle()
{
    if (this->is_handling() == false)
    {
        _previous_handler = SIG_ERR;
        _sig = -1;
        return {};
    }
    if (std::signal(_sig, _previous_handler) == SIG_ERR)
    {
        _previous_handler = SIG_ERR;
        _sig = -1;
        return std::unexpected(Error::from_errno("could not unhandle signal {}", _sig));
    }

    _previous_handler = SIG_ERR;
    _sig = -1;

    return {};
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

} // namespace sihd::sys
