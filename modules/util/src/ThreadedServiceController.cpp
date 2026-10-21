#include <sihd/util/ThreadedServiceController.hpp>

namespace sihd::util
{

using Transition = AService::Transition;

ThreadedServiceController::ThreadedServiceController()
{
    current_state = Stopped;
}

Transition ThreadedServiceController::op_start(AService::Operation op)
{
    std::lock_guard l(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            if (current_state == Stopped || current_state == StartFailure)
            {
                current_state = Starting;
                return Transition::proceed;
            }
            return Transition::refused;
        case AService::Operation::Stop:
            if (current_state == Running || current_state == StopFailure)
            {
                current_state = Stopping;
                return Transition::proceed;
            }
            if (current_state == Stopped || current_state == Stopping)
                return Transition::done;
            return Transition::refused;
        default:
            break;
    }
    return Transition::proceed;
}

bool ThreadedServiceController::op_end(AService::Operation op, bool status)
{
    std::lock_guard l(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            current_state = status ? Running : StartFailure;
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
