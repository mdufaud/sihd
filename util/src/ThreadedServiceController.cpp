#include <sihd/util/ThreadedServiceController.hpp>

namespace sihd::util
{

ThreadedServiceController::ThreadedServiceController()
{
    current_state = STOPPED;
}

bool ThreadedServiceController::op_start(AService::Operation op)
{
    std::lock_guard l(_state_mutex);
    switch (op)
    {
        case AService::Operation::START:
            if (current_state == STOPPED || current_state == ERROR)
            {
                current_state = STARTING;
                return true;
            }
            return false;
        case AService::Operation::STOP:
            if (current_state == RUNNING)
            {
                current_state = STOPPING;
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
        case AService::Operation::START:
            current_state = status ? RUNNING : ERROR;
            break;
        case AService::Operation::STOP:
            current_state = status ? STOPPED : ERROR;
            break;
        default:
            break;
    }
    return true;
}

} // namespace sihd::util
