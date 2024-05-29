#ifndef __SIHD_UTIL_THREADEDSERVICE_HPP__
#define __SIHD_UTIL_THREADEDSERVICE_HPP__

#include <sihd/util/AService.hpp>
#include <sihd/util/ThreadedServiceController.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::util
{

class ThreadedService: public sihd::util::AService
{
    public:
        ThreadedService();
        virtual ~ThreadedService();

        // only if service started a thread
        void set_start_synchronised(bool active);

        virtual sihd::util::AService::IServiceController *service_ctrl() override { return &_service_controller; }

    protected:
        virtual bool on_start() = 0;
        virtual bool on_stop() = 0;

        virtual bool do_start() override;
        virtual bool do_stop() override;

        void set_service_nb_thread(uint8_t n);
        // if service starts a thread - call this as soon as thread is started
        void notify_service_thread_started();

        ThreadedServiceController _service_controller;

    private:
        Synchronizer _synchro;
        uint8_t _number_of_threads;
        bool _start_synchronised;
};

} // namespace sihd::util

#endif
