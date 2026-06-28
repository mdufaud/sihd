#ifndef __SIHD_UTIL_BLOCKINGSERVICECONTROLLER_HPP__
#define __SIHD_UTIL_BLOCKINGSERVICECONTROLLER_HPP__

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
            Error,
        };

        virtual bool op_start(AService::Operation op);
        virtual bool op_end(AService::Operation op, bool status);

        State current_state;

    protected:

    private:
        std::mutex _state_mutex;
};

} // namespace sihd::util

#endif
