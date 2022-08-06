#ifndef __SIHD_UTIL_SIGWAITER_HPP__
# define __SIHD_UTIL_SIGWAITER_HPP__

#include <signal.h>

#include <sihd/util/IHandler.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class SigWaiter: public IHandler<int>
{
    public:
        SigWaiter(int sig = SIGINT);
        virtual ~SigWaiter();

    protected:
        void handle(int sig);

    private:
        int _sig;
        Waitable _waitable;
};

}

#endif