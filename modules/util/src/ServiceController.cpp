#include <sihd/util/ServiceController.hpp>

namespace
{

using SM = sihd::util::StateMachine<sihd::util::ServiceController::State, sihd::util::AService::Operation>;
using St = sihd::util::ServiceController;
using Op = sihd::util::AService;

const SM & default_statemachine()
{
    static const SM sm = [] {
        SM m(St::None);
        m.set_transitions_map({
            // none -> setup -> configured
            {SM::pack_key(St::None,         Op::Setup),   St::Configuring},
            {SM::pack_key(St::Configuring,  Op::Error),   St::Error},
            {SM::pack_key(St::Configuring,  Op::Success), St::Configured},
            // configured -> init -> stopped
            {SM::pack_key(St::Configured,   Op::Init),    St::Initializing},
            {SM::pack_key(St::Initializing, Op::Error),   St::Error},
            {SM::pack_key(St::Initializing, Op::Success), St::Stopped},
            // stopped -> start -> running
            {SM::pack_key(St::Stopped,      Op::Start),   St::Starting},
            {SM::pack_key(St::Starting,     Op::Error),   St::Error},
            {SM::pack_key(St::Starting,     Op::Success), St::Running},
            // running -> stop -> stopped
            {SM::pack_key(St::Running,      Op::Stop),    St::Stopping},
            {SM::pack_key(St::Stopping,     Op::Error),   St::Error},
            {SM::pack_key(St::Stopping,     Op::Success), St::Stopped},
            // configured/stopped -> reset -> none
            {SM::pack_key(St::Configured,   Op::Reset),   St::Resetting},
            {SM::pack_key(St::Stopped,      Op::Reset),   St::Resetting},
            {SM::pack_key(St::Resetting,    Op::Error),   St::Error},
            {SM::pack_key(St::Resetting,    Op::Success), St::None},
        });
        m.set_states_names_map({
            {St::None,         "none"},
            {St::Configuring,  "configuring"},
            {St::Configured,   "configured"},
            {St::Initializing, "initializing"},
            {St::Starting,     "starting"},
            {St::Running,      "running"},
            {St::Stopping,     "stopping"},
            {St::Stopped,      "stopped"},
            {St::Resetting,    "resetting"},
        });
        m.set_events_names_map({
            {Op::Setup,   "setup"},
            {Op::Init,    "init"},
            {Op::Start,   "start"},
            {Op::Stop,    "stop"},
            {Op::Reset,   "reset"},
            {Op::Success, "success"},
            {Op::Error,   "error"},
        });
        return m;
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

ServiceController::ServiceController(): statemachine(default_statemachine())
{
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