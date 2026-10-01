#include <sihd/util/ThreadedServiceController.hpp>

namespace sihd::util
{

ThreadedServiceController::ThreadedServiceController()
{
    current_state = Stopped;
}

bool ThreadedServiceController::op_start(AService::Operation op)
{
    std::lock_guard l(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            if (current_state == Stopped || current_state == Failure)
            {
                current_state = Starting;
                return true;
            }
            return false;
        case AService::Operation::Stop:
            if (current_state == Running)
            {
                current_state = Stopping;
                return true;
            }
            return false;
        default:
            break;
    }
    return true;
}

bool ThreadedServiceController::op_end(AService::Operation op, bool status)
{
    std::lock_guard l(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            current_state = status ? Running : Failure;
            break;
        case AService::Operation::Stop:
            current_state = status ? Stopped : Failure;
            break;
        default:
            break;
    }
    return true;
}

} // namespace sihd::util
