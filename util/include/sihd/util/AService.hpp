#ifndef __SIHD_UTIL_ASERVICE_HPP__
#define __SIHD_UTIL_ASERVICE_HPP__

#include <sihd/util/Observable.hpp>

namespace sihd::util
{

class AService: public Observable<AService>
{
    public:
        virtual ~AService() {};

        enum Operation
        {
            SETUP = 0,
            INIT,
            START,
            STOP,
            RESET,
            SUCCESS = 254,
            ERROR = 255,
        };

        class IServiceController
        {
            public:
                virtual bool op_start(Operation op) = 0;
                virtual bool op_end(Operation op, bool status) = 0;
        };
        virtual IServiceController *service_ctrl() { return nullptr; };

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
};

} // namespace sihd::util

#endif