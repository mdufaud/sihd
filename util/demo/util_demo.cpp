#include <sihd/util/Logger.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/Runnable.hpp>

#include <cxxopts.hpp>

using namespace sihd::util;

namespace demo {

SIHD_NEW_LOGGER("demo");

void worker(double frequency)
{
    Runnable printer([] {
        SIHD_LOG(info, "time: {}", Timestamp::now().local_format());
        return true;
    });

    StepWorker worker;
    worker.set_runnable(&printer);
    worker.set_frequency(frequency);
    worker.start_sync_worker("worker");

    SigWaiter sigwait;

    worker.stop_worker();
}

void os()
{
    SIHD_LOG(info, "pid: {}", os::pid());
}

}

int main(int argc, char **argv)
{
    LoggerManager::basic();

    cxxopts::Options options(argv[0], "Testing utility for module util");
    options.add_options()
      ("h,help", "Prints usage")
      ("f,worker-frequency", "Change the worker execution frequency in HZ", cxxopts::value<double>()->default_value("10.0"));

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
      fmt::print(options.help());
      return 0;
    }

    demo::os();
    demo::worker(result["worker-frequency"].as<double>());

    LoggerManager::clear_loggers();
    return 0;
}
