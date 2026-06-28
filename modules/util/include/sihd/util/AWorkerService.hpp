#ifndef __SIHD_UTIL_AWORKERSERVICE_HPP__
#define __SIHD_UTIL_AWORKERSERVICE_HPP__

#include <atomic>

#include <sihd/util/AThreadedService.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::util
{

class AWorkerService: public AThreadedService,
                      public IRunnable
{
    public:
        AWorkerService(std::string_view name);
        virtual ~AWorkerService();

        virtual bool is_running() const override;

    protected:
        virtual bool on_start() override;
        virtual bool on_stop() override;
        bool run() override;

        virtual bool on_work_start() = 0;
        virtual bool on_work_stop() = 0;

        std::atomic<bool> stop_requested;

    private:
        Worker _worker;
};

} // namespace sihd::util

#endif
