#ifndef __SIHD_UTIL_THREADEDSERVICECONTROLLER_HPP__
#define __SIHD_UTIL_THREADEDSERVICECONTROLLER_HPP__

#include <mutex>

#include <sihd/util/AService.hpp>

namespace sihd::util
{

class ThreadedServiceController: public sihd::util::AService::IServiceController
{
    public:
        ThreadedServiceController();
        virtual ~ThreadedServiceController() = default;

        enum State
        {
            Starting,
            Running,
            Stopping,
            Stopped,
            Error,
        };

        virtual bool op_start(AService::Operation op);
        virtual bool op_end(AService::Operation op, bool status);

        State current_state;

    private:
        std::mutex _state_mutex;
};

} // namespace sihd::util

#endif
