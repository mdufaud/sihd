#ifndef __SIHD_UTIL_THREADEDSERVICE_HPP__
#define __SIHD_UTIL_THREADEDSERVICE_HPP__

#include <barrier>

#include <sihd/util/AService.hpp>
#include <sihd/util/ThreadedServiceController.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::util
{

class AThreadedService: public sihd::util::AService
{
    public:
        AThreadedService(std::string_view name);
        virtual ~AThreadedService();

        // only if service started a thread
        void set_start_synchronised(bool active);

        virtual sihd::util::AService::IServiceController *service_ctrl() override
        {
            return &_service_controller;
        }

    protected:
        virtual bool on_start() = 0;
        virtual bool on_stop() = 0;

        virtual bool do_start() override;
        virtual bool do_stop() override;

        void set_service_nb_thread(uint8_t n);
        // if service starts a thread - call this as soon as thread is started
        void notify_service_thread_started();

        const std::string & thread_name() const;

        ThreadedServiceController _service_controller;

    private:
        std::unique_ptr<std::barrier<>> _barrier_ptr;
        std::string _thread_name;
        bool _start_synchronised;
};

} // namespace sihd::util

#endif
