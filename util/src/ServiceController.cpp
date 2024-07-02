#include <sihd/util/ServiceController.hpp>

namespace sihd::util
{

ServiceController::ServiceController(const StateMachine<State, AService::Operation> & to_copy_statemachine):
    statemachine(None)
{
    statemachine.set_transitions_map(to_copy_statemachine.transitions_map());
    statemachine.set_states_names_map(to_copy_statemachine.states_names_map());
    statemachine.set_events_names_map(to_copy_statemachine.events_names_map());
}

ServiceController::ServiceController(): statemachine(None)
{
    // none -> setup -> configured
    statemachine.add_transition(None, AService::Setup, Configuring);
    statemachine.add_transition(Configuring, AService::Error, Error);
    statemachine.add_transition(Configuring, AService::Success, Configured);

    // configured -> init -> stopped
    statemachine.add_transition(Configured, AService::Init, Initializing);
    statemachine.add_transition(Initializing, AService::Error, Error);
    statemachine.add_transition(Initializing, AService::Success, Stopped);

    // stopped -> start -> running
    statemachine.add_transition(Stopped, AService::Start, Starting);
    statemachine.add_transition(Starting, AService::Error, Error);
    statemachine.add_transition(Starting, AService::Success, Running);

    // running -> stop -> stopped
    statemachine.add_transition(Running, AService::Stop, Stopping);
    statemachine.add_transition(Stopping, AService::Error, Error);
    statemachine.add_transition(Stopping, AService::Success, Stopped);

    // configured -> reset -> none
    statemachine.add_transition(Configured, AService::Reset, Resetting);

    // stopped -> reset -> none
    statemachine.add_transition(Stopped, AService::Reset, Resetting);
    statemachine.add_transition(Resetting, AService::Error, Error);
    statemachine.add_transition(Resetting, AService::Success, None);

    statemachine.set_state_name(None, "none");
    statemachine.set_state_name(Configuring, "configuring");
    statemachine.set_state_name(Configured, "configured");
    statemachine.set_state_name(Initializing, "initializing");
    statemachine.set_state_name(Starting, "starting");
    statemachine.set_state_name(Running, "running");
    statemachine.set_state_name(Stopping, "stopping");
    statemachine.set_state_name(Stopped, "stopped");
    statemachine.set_state_name(Resetting, "resetting");

    statemachine.set_event_name(AService::Setup, "setup");
    statemachine.set_event_name(AService::Init, "init");
    statemachine.set_event_name(AService::Start, "start");
    statemachine.set_event_name(AService::Stop, "stop");
    statemachine.set_event_name(AService::Reset, "reset");
    statemachine.set_event_name(AService::Success, "success");
    statemachine.set_event_name(AService::Error, "error");
}

void ServiceController::optional_setup()
{
    statemachine.add_transition(None, AService::Init, Initializing);
}

void ServiceController::optional_init()
{
    statemachine.add_transition(None, AService::Start, Starting);
}

bool ServiceController::op_start(AService::Operation op)
{
    bool ret;
    {
        std::lock_guard l(_state_mutex);
        ret = statemachine.transition(op);
    }
    if (ret)
        this->notify_observers(this);
    return ret;
}

bool ServiceController::op_end([[maybe_unused]] AService::Operation op, bool status)
{
    bool ret;
    {
        std::lock_guard l(_state_mutex);
        ret = statemachine.transition(status ? AService::Success : AService::Error);
    }
    this->notify_observers(this);
    return ret;
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
        case Error:
            return "error";
        default:
            return "none";
    }
}

} // namespace sihd::util