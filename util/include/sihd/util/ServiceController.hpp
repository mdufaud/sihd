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
        ServiceController();
        virtual ~ServiceController() {};

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

        virtual bool    op_start(AService::Operation op);
        virtual bool    op_end(AService::Operation op, bool status);

        State   get_state()
        {
            return statemachine.get_state();
        }

        void    optionnal_setup();

        StateMachine<State, AService::Operation>    statemachine;

    protected:
    
    private:
};

}

#endif 