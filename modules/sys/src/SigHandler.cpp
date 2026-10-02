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

    auto handled = signal::handle(sig, &_previous_handler);
    if (!handled)
    {
        _previous_handler = SIG_ERR;
        _sig = -1;
    }
    return handled;
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
        const int sig = _sig;
        _previous_handler = SIG_ERR;
        _sig = -1;
        return std::unexpected(Error::from_errno("could not unhandle signal {}", sig));
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
