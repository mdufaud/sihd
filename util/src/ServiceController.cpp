#include <sihd/util/ServiceController.hpp>

namespace sihd::util
{

#define SET_STATE_NAME(name) statemachine.set_state_name(name, #name);
#define SET_EVENT_NAME(name) statemachine.set_event_name(AService::name, #name);

ServiceController::ServiceController(): statemachine(NONE)
{
    // none -> setup -> setuped
    statemachine.add_transition(NONE, AService::SETUP, SETUPING);
    statemachine.add_transition(SETUPING, AService::ERROR, ERROR);
    statemachine.add_transition(SETUPING, AService::SUCCESS, SETUPED);

    // setuped -> init -> stopped
    statemachine.add_transition(SETUPED, AService::INIT, INITIALIZING);
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

    // setuped -> reset -> none
    statemachine.add_transition(SETUPED, AService::RESET, RESETTING);

    // stopped -> reset -> none
    statemachine.add_transition(STOPPED, AService::RESET, RESETTING);
    statemachine.add_transition(RESETTING, AService::ERROR, ERROR);
    statemachine.add_transition(RESETTING, AService::SUCCESS, NONE);

    SET_STATE_NAME(NONE);
    SET_STATE_NAME(SETUPING);
    SET_STATE_NAME(SETUPED);
    SET_STATE_NAME(INITIALIZING);
    SET_STATE_NAME(STARTING);
    SET_STATE_NAME(RUNNING);
    SET_STATE_NAME(STOPPING);
    SET_STATE_NAME(STOPPED);
    SET_STATE_NAME(RESETTING);

    SET_EVENT_NAME(SETUP);
    SET_EVENT_NAME(INIT);
    SET_EVENT_NAME(START);
    SET_EVENT_NAME(STOP);
    SET_EVENT_NAME(RESET);
    SET_EVENT_NAME(SUCCESS);
    SET_EVENT_NAME(ERROR);
}

bool    ServiceController::op_start(AService::Operation op)
{
    bool ret = statemachine.transition(op);
    if (ret)
        this->notify_observers(this);
    return ret;
}

bool    ServiceController::op_end(AService::Operation op, bool status)
{
    (void)op;
    bool ret = statemachine.transition(status ? AService::SUCCESS : AService::ERROR);
    this->notify_observers(this);
    return ret;

}

}