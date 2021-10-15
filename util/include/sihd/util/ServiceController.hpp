#ifndef __SIHD_UTIL_SERVICECONTROLLER_HPP__
# define __SIHD_UTIL_SERVICECONTROLLER_HPP__

# include <sihd/util/AService.hpp>
# include <sihd/util/StateMachine.hpp>

namespace sihd::util
{

class ServiceController:    virtual public AService::IServiceController,
                            public Observable<ServiceController>
{
    public:
        enum State
        {
            NONE = 0,
            CONFIGURING,
            CONFIGURED,
            INITIALIZING,
            STARTING,
            RUNNING,
            STOPPING,
            STOPPED,
            RESETTING,
            ERROR = 255,
        };

        ServiceController();
        ServiceController(const StateMachine<State, AService::Operation> & to_copy_statemachine);
        virtual ~ServiceController() {};

        virtual bool op_start(AService::Operation op);
        virtual bool op_end(AService::Operation op, bool status);

        State get_state()
        {
            return statemachine.get_state();
        }

        void optionnal_setup();

        StateMachine<State, AService::Operation> statemachine;

    protected:
    
    private:
};

}

#endif 