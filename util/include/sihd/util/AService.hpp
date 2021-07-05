#ifndef __SIHD_UTIL_ASERVICE_HPP__
# define __SIHD_UTIL_ASERVICE_HPP__

# include <sihd/util/Observable.hpp>

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

        class IController
        {
            public:
                virtual bool    op_start(Operation op) = 0;
                virtual bool    op_end(Operation op, bool status) = 0;
        };
        virtual IController *get_ctrl() { return nullptr; };

        virtual bool    setup();
        virtual bool    init();
        virtual bool    start();
        virtual bool    stop();
        virtual bool    reset();

        virtual bool    is_running() const = 0;

    protected:
        virtual bool    on_setup();
        virtual bool    on_init();
        virtual bool    on_start();
        virtual bool    on_stop();
        virtual bool    on_reset();
 
    private:

};

}

#endif 