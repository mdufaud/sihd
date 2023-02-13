#include <sihd/util/Logger.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/SigWaiter.hpp>
#include <sihd/util/Runnable.hpp>

#include <argparse/argparse.hpp>

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
    argparse::ArgumentParser program("sihd_util_demo");

    program.add_argument("--worker-frequency")
      .help("Change the worker execution frequency in HZ")
      .default_value(10.0)
      .scan<'g', double>();

    try {
      program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
      std::cerr << err.what() << std::endl;
      std::cerr << program;
      std::exit(1);
    }

    LoggerManager::basic();

    demo::os();
    demo::worker(program.get<double>("worker-frequency"));

    LoggerManager::clear_loggers();
    return 0;
}
