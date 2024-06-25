#ifndef __SIHD_UTIL_ABLOCKINGSERVICE_HPP__
#define __SIHD_UTIL_ABLOCKINGSERVICE_HPP__

#include <mutex>

#include <sihd/util/AService.hpp>
#include <sihd/util/BlockingServiceController.hpp>

namespace sihd::util
{

class ABlockingService: public sihd::util::AService
{
    public:
        ABlockingService();
        virtual ~ABlockingService() = default;

        void set_service_wait_stop(bool active);

        virtual bool is_running() const override;

        virtual sihd::util::AService::IServiceController *service_ctrl() override { return &_service_controller; }

    protected:
        virtual bool on_start() = 0;
        virtual bool on_stop() = 0;

        virtual bool do_start() override;
        virtual bool do_stop() override;

        void service_wait_stop() const;

        BlockingServiceController _service_controller;

    private:
        mutable std::mutex _running_mutex;
        bool _running;
        bool _wait_stop;
};

} // namespace sihd::util

#endif
