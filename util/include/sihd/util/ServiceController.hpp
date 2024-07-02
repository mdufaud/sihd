#ifndef __SIHD_UTIL_SERVICECONTROLLER_HPP__
#define __SIHD_UTIL_SERVICECONTROLLER_HPP__

#include <mutex>

#include <sihd/util/AService.hpp>
#include <sihd/util/StateMachine.hpp>

namespace sihd::util
{

class ServiceController: public AService::IServiceController,
                         public Observable<ServiceController>
{
    public:
        enum State
        {
            None = 0,
            Configuring,
            Configured,
            Initializing,
            Starting,
            Running,
            Stopping,
            Stopped,
            Resetting,
            Error = 255,
        };

        ServiceController();
        ServiceController(const StateMachine<State, AService::Operation> & to_copy_statemachine);
        virtual ~ServiceController() = default;

        virtual bool op_start(AService::Operation op);
        virtual bool op_end(AService::Operation op, bool status);

        State state() const { return statemachine.state(); }

        void optional_setup();
        void optional_init();

        static const char *state_str(State state);

        StateMachine<State, AService::Operation> statemachine;

    protected:

    private:
        std::mutex _state_mutex;
};

} // namespace sihd::util

#endif