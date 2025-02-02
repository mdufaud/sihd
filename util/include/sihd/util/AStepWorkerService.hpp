#ifndef __SIHD_UTIL_ASTEPWORKERSERVICE_HPP__
#define __SIHD_UTIL_ASTEPWORKERSERVICE_HPP__

#include <sihd/util/AThreadedService.hpp>
#include <sihd/util/StepWorker.hpp>

namespace sihd::util
{

class AStepWorkerService: public AThreadedService,
                          public IRunnable
{
    public:
        AStepWorkerService(std::string_view name);
        virtual ~AStepWorkerService();

        bool set_step_frequency(double frequency);
        virtual bool is_running() const override;

    protected:
        virtual bool on_start() override;
        virtual bool on_stop() override;

        bool run() override;

        virtual bool on_work_setup() = 0;
        virtual bool on_work_start() = 0;
        virtual bool on_work_stop() = 0;
        virtual bool on_work_teardown() = 0;

    private:
        StepWorker _step_worker;
};

} // namespace sihd::util

#endif
