#include <sihd/util/ServiceController.hpp>

namespace
{

using SM = sihd::util::StateMachine<sihd::util::ServiceController::State, sihd::util::AService::Operation>;
using St = sihd::util::ServiceController;
using Op = sihd::util::AService;

const SM & default_statemachine()
{
    static const SM sm = [] {
        SM machine(St::None);
        machine.set_transitions_map({
            {SM::pack_key(St::None, Op::Setup), St::Configuring},
            {SM::pack_key(St::Configuring, Op::Failure), St::Failure},
            {SM::pack_key(St::Configuring, Op::Success), St::Configured},
            {SM::pack_key(St::Configured, Op::Init), St::Initializing},
            {SM::pack_key(St::Initializing, Op::Failure), St::Failure},
            {SM::pack_key(St::Initializing, Op::Success), St::Stopped},
            {SM::pack_key(St::Stopped, Op::Start), St::Starting},
            {SM::pack_key(St::Starting, Op::Failure), St::StartFailure},
            {SM::pack_key(St::Starting, Op::Success), St::Running},
            {SM::pack_key(St::Running, Op::Stop), St::Stopping},
            {SM::pack_key(St::Stopping, Op::Failure), St::StopFailure},
            {SM::pack_key(St::Stopping, Op::Success), St::Stopped},
            // a failed start or stop is retried by the same operation
            {SM::pack_key(St::StartFailure, Op::Start), St::Starting},
            {SM::pack_key(St::StopFailure, Op::Stop), St::Stopping},
            {SM::pack_key(St::Configured, Op::Reset), St::Resetting},
            {SM::pack_key(St::Stopped, Op::Reset), St::Resetting},
            {SM::pack_key(St::Resetting, Op::Failure), St::Failure},
            {SM::pack_key(St::Resetting, Op::Success), St::None},
        });
        machine.set_states_names_map({
            {St::None, "none"},
            {St::Configuring, "configuring"},
            {St::Configured, "configured"},
            {St::Initializing, "initializing"},
            {St::Starting, "starting"},
            {St::Running, "running"},
            {St::Stopping, "stopping"},
            {St::Stopped, "stopped"},
            {St::Resetting, "resetting"},
        });
        machine.set_events_names_map({
            {Op::Setup, "setup"},
            {Op::Init, "init"},
            {Op::Start, "start"},
            {Op::Stop, "stop"},
            {Op::Reset, "reset"},
            {Op::Success, "success"},
            {Op::Failure, "error"},
        });
        return machine;
    }();
    return sm;
}

} // namespace

namespace sihd::util
{

ServiceController::ServiceController(const StateMachine<State, AService::Operation> & to_copy_statemachine):
    statemachine(to_copy_statemachine)
{
}

ServiceController::ServiceController(): statemachine(default_statemachine()) {}

void ServiceController::optional_setup()
{
    statemachine.add_transition(None, AService::Init, Initializing);
}

void ServiceController::optional_init()
{
    statemachine.add_transition(None, AService::Start, Starting);
}

AService::Transition ServiceController::op_start(AService::Operation op)
{
    bool ret;
    {
        std::lock_guard lock(_state_mutex);
        const State state = statemachine.state();
        if (op == AService::Stop && (state == Stopping || state == Stopped))
            return AService::Transition::done;
        ret = statemachine.transition(op);
    }
    if (!ret)
        return AService::Transition::refused;
    this->notify_observers(this);
    return AService::Transition::proceed;
}

bool ServiceController::op_end([[maybe_unused]] AService::Operation op, bool status)
{
    bool ret;
    {
        std::lock_guard lock(_state_mutex);
        ret = statemachine.transition(status ? AService::Success : AService::Failure);
    }
    this->notify_observers(this);
    return ret;
}

ServiceController::State ServiceController::state() const
{
    std::lock_guard lock(_state_mutex);
    return statemachine.state();
}

AService::Operation ServiceController::last_event() const
{
    std::lock_guard lock(_state_mutex);
    return statemachine.last_event();
}

const char *ServiceController::state_str(State state)
{
    switch (state)
    {
        case Configuring:
            return "configuring";
        case Configured:
            return "configured";
        case Initializing:
            return "initializing";
        case Starting:
            return "starting";
        case Running:
            return "running";
        case Stopping:
            return "stopping";
        case Stopped:
            return "stopped";
        case Resetting:
            return "resetting";
        case StartFailure:
            return "start error";
        case StopFailure:
            return "stop error";
        case Failure:
            return "error";
        default:
            return "none";
    }
}

} // namespace sihd::util