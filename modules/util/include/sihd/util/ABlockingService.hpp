#ifndef __SIHD_UTIL_ABLOCKINGSERVICE_HPP__
#define __SIHD_UTIL_ABLOCKINGSERVICE_HPP__

#include <atomic>
#include <mutex>
#include <thread>

#include <sihd/util/AService.hpp>
#include <sihd/util/BlockingServiceController.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class ABlockingService: public sihd::util::AService
{
    public:
        ABlockingService();
        virtual ~ABlockingService() = default;

        void set_service_wait_stop(bool active);

        virtual bool wait_ready(Duration timeout) const;
        virtual bool is_ready() const;
        virtual bool is_running() const override;

        virtual sihd::util::AService::IServiceController *service_ctrl() override { return &_service_controller; }

    protected:
        virtual bool on_start() = 0;
        virtual bool on_stop() = 0;

        virtual bool do_start() override;
        virtual bool do_stop() override;

        void service_wait_stop() const;
        void service_set_ready();
        bool service_stop_requested() const;

        // a stop landing before an inner poll ran found it stopped: the poll ends on its next tick instead
        template <typename PollType>
        bool service_relay_stop(PollType *poll) const
        {
            const bool relayed = this->is_running() && this->service_stop_requested();
            if (relayed)
                poll->request_stop();
            return relayed;
        }

        BlockingServiceController _service_controller;

    private:
        mutable std::mutex _running_mutex;
        std::atomic<bool> _running;
        std::atomic<std::thread::id> _running_thread;
        mutable Waitable _waitable;
        std::atomic<bool> _ready;
        std::atomic<bool> _wait_stop;
};

} // namespace sihd::util

#endif
