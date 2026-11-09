#ifndef __SIHD_UTIL_BLOCKINGSERVICECONTROLLER_HPP__
#define __SIHD_UTIL_BLOCKINGSERVICECONTROLLER_HPP__

#include <atomic>

#include <sihd/util/AService.hpp>

namespace sihd::util
{

class BlockingServiceController: public sihd::util::AService::IServiceController
{
    public:
        BlockingServiceController();
        virtual ~BlockingServiceController() = default;

        enum State
        {
            Running,
            Stopped,
            StartFailure,
            StopFailure,
            Failure,
        };

        virtual AService::Transition op_start(AService::Operation op);
        virtual bool op_end(AService::Operation op, bool status);

        bool stop_requested() const;

        State current_state;

    protected:

    private:
        std::mutex _state_mutex;
        std::atomic<bool> _stop_requested;
};

} // namespace sihd::util

#endif
