#ifndef __SIHD_UTIL_AWORKERSERVICE_HPP__
#define __SIHD_UTIL_AWORKERSERVICE_HPP__

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
        bool is_work_stop_requested() const;

        virtual bool on_start() override;
        virtual bool on_stop() override;
        bool run() override;

        virtual bool on_work_start() = 0;
        virtual bool on_work_stop() = 0;

    private:
        bool _stop_requested;
        Worker _worker;
};

} // namespace sihd::util

#endif
