#include <sihd/util/BlockingServiceController.hpp>

namespace sihd::util
{

using Transition = AService::Transition;

BlockingServiceController::BlockingServiceController(): _stop_requested(false)
{
    current_state = Stopped;
}

bool BlockingServiceController::stop_requested() const
{
    return _stop_requested;
}

Transition BlockingServiceController::op_start(AService::Operation op)
{
    std::lock_guard lock(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            if (current_state == Stopped || current_state == StartFailure)
            {
                current_state = Running;
                // set and cleared under the state lock: a stop right after this start is never erased
                _stop_requested = false;
                return Transition::proceed;
            }
            return Transition::refused;
        case AService::Operation::Stop:
            if (current_state == Running || current_state == StopFailure)
            {
                current_state = Stopped;
                _stop_requested = true;
                return Transition::proceed;
            }
            return current_state == Stopped ? Transition::done : Transition::refused;
        default:
            break;
    }
    return Transition::proceed;
}

bool BlockingServiceController::op_end(AService::Operation op, bool status)
{
    std::lock_guard lock(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            current_state = status ? Stopped : StartFailure;
            break;
        case AService::Operation::Stop:
            current_state = status ? Stopped : StopFailure;
            break;
        default:
            if (!status)
                current_state = Failure;
            else if (current_state == Failure)
                current_state = Stopped;
            break;
    }
    return true;
}

} // namespace sihd::util
