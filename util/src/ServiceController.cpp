#include <sihd/util/ServiceController.hpp>

namespace sihd::util
{

ServiceController::ServiceController(const StateMachine<State, AService::Operation> & to_copy_statemachine):
    statemachine(NONE)
{
    statemachine.set_transitions_map(to_copy_statemachine.transitions_map());
    statemachine.set_states_names_map(to_copy_statemachine.states_names_map());
    statemachine.set_events_names_map(to_copy_statemachine.events_names_map());
}

ServiceController::ServiceController(): statemachine(NONE)
{
    // none -> setup -> configured
    statemachine.add_transition(NONE, AService::SETUP, CONFIGURING);
    statemachine.add_transition(CONFIGURING, AService::ERROR, ERROR);
    statemachine.add_transition(CONFIGURING, AService::SUCCESS, CONFIGURED);

    // configured -> init -> stopped
    statemachine.add_transition(CONFIGURED, AService::INIT, INITIALIZING);
    statemachine.add_transition(INITIALIZING, AService::ERROR, ERROR);
    statemachine.add_transition(INITIALIZING, AService::SUCCESS, STOPPED);

    // stopped -> start -> running
    statemachine.add_transition(STOPPED, AService::START, STARTING);
    statemachine.add_transition(STARTING, AService::ERROR, ERROR);
    statemachine.add_transition(STARTING, AService::SUCCESS, RUNNING);

    // running -> stop -> stopped
    statemachine.add_transition(RUNNING, AService::STOP, STOPPING);
    statemachine.add_transition(STOPPING, AService::ERROR, ERROR);
    statemachine.add_transition(STOPPING, AService::SUCCESS, STOPPED);

    // configured -> reset -> none
    statemachine.add_transition(CONFIGURED, AService::RESET, RESETTING);

    // stopped -> reset -> none
    statemachine.add_transition(STOPPED, AService::RESET, RESETTING);
    statemachine.add_transition(RESETTING, AService::ERROR, ERROR);
    statemachine.add_transition(RESETTING, AService::SUCCESS, NONE);

    statemachine.set_state_name(NONE, "none");
    statemachine.set_state_name(CONFIGURING, "configuring");
    statemachine.set_state_name(CONFIGURED, "configured");
    statemachine.set_state_name(INITIALIZING, "initializing");
    statemachine.set_state_name(STARTING, "starting");
    statemachine.set_state_name(RUNNING, "running");
    statemachine.set_state_name(STOPPING, "stopping");
    statemachine.set_state_name(STOPPED, "stopped");
    statemachine.set_state_name(RESETTING, "resetting");

    statemachine.set_event_name(AService::SETUP, "setup");
    statemachine.set_event_name(AService::INIT, "init");
    statemachine.set_event_name(AService::START, "start");
    statemachine.set_event_name(AService::STOP, "stop");
    statemachine.set_event_name(AService::RESET, "reset");
    statemachine.set_event_name(AService::SUCCESS, "success");
    statemachine.set_event_name(AService::ERROR, "error");
}

void ServiceController::optional_setup()
{
    statemachine.add_transition(NONE, AService::INIT, INITIALIZING);
}

void ServiceController::optional_init()
{
    statemachine.add_transition(NONE, AService::START, STARTING);
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
        ret = statemachine.transition(status ? AService::SUCCESS : AService::ERROR);
    }
    this->notify_observers(this);
    return ret;
}

const char *ServiceController::state_str(State state)
{
    switch (state)
    {
        case CONFIGURING:
            return "configuring";
        case CONFIGURED:
            return "configured";
        case INITIALIZING:
            return "initializing";
        case STARTING:
            return "starting";
        case RUNNING:
            return "running";
        case STOPPING:
            return "stopping";
        case STOPPED:
            return "stopped";
        case RESETTING:
            return "resetting";
        case ERROR:
            return "error";
        default:
            return "none";
    }
}

} // namespace sihd::util