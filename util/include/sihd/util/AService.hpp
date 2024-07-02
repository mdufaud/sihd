#ifndef __SIHD_UTIL_ASERVICE_HPP__
#define __SIHD_UTIL_ASERVICE_HPP__

#include <sihd/util/Observable.hpp>
#include <sihd/util/ObservableDelegate.hpp>

namespace sihd::util
{

class AService
{
    public:
        AService();
        virtual ~AService() = default;

        enum Operation
        {
            Setup = 0,
            Init,
            Start,
            Stop,
            Reset,
            Success = 254,
            Error = 255,
        };

        class IServiceController
        {
            public:
                virtual bool op_start(Operation op) = 0;
                virtual bool op_end(Operation op, bool status) = 0;
        };
        virtual IServiceController *service_ctrl() { return nullptr; };

        // enable observation but prevent calling notify
        ObservableDelegate<AService>::Protector service_state() { return _service_state.get_protected_obs(); }

        virtual bool setup();
        virtual bool init();
        virtual bool start();
        virtual bool stop();
        virtual bool reset();

        virtual bool is_running() const = 0;

    protected:
        virtual bool do_setup();
        virtual bool do_init();
        virtual bool do_start();
        virtual bool do_stop();
        virtual bool do_reset();

    private:
        // remove the need for some services to do this: service->Observable<T>::add_observer(this)
        // by delegating the observable state to a composite object
        ObservableDelegate<AService> _service_state;
};

} // namespace sihd::util

#endif