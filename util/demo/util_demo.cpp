#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/Runnable.hpp>

using namespace sihd::util;

namespace demo {

SIHD_NEW_LOGGER("demo");

void worker()
{
    Runnable printer([] {
        SIHD_LOGF(info, "time: {}", Timestamp(Clock::default_clock.now()).local_format());
        return true;
    });

    StepWorker worker;
    worker.set_runnable(&printer);
    worker.set_frequency(10);
    worker.start_sync_worker("worker");

    SigWaiter sigwait;

    worker.stop_worker();
}

void os()
{
    SIHD_LOGF(info, "pid: {}", OS::pid());
}

}

int main()
{
    LoggerManager::basic();

    demo::os();
    demo::worker();

    LoggerManager::clear_loggers();
    return 0;
}