#include <sihd/util/BlockingServiceController.hpp>

namespace sihd::util
{

BlockingServiceController::BlockingServiceController()
{
    current_state = Stopped;
}

bool BlockingServiceController::op_start(AService::Operation op)
{
    std::lock_guard l(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            if (current_state == Stopped || current_state == Error)
            {
                current_state = Running;
                return true;
            }
            return false;
        case AService::Operation::Stop:
            if (current_state == Running)
            {
                current_state = Stopped;
                return true;
            }
            return false;
        default:
            break;
    }
    return true;
}

bool BlockingServiceController::op_end(AService::Operation op, bool status)
{
    std::lock_guard l(_state_mutex);
    switch (op)
    {
        case AService::Operation::Start:
            current_state = status ? Running : Error;
            break;
        case AService::Operation::Stop:
            current_state = status ? Stopped : Error;
            break;
        default:
            break;
    }
    return true;
}

} // namespace sihd::util
