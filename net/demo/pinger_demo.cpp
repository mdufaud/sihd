#include <signal.h>

#include <sihd/util/Handler.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/net/Pinger.hpp>

using namespace sihd::util;
using namespace sihd::net;

int main(int argc, char **argv)
{
    sihd::util::LoggerManager::console();
    Logger log("demo");
    Pinger pinger("pinger");

    if (pinger.open(false) == false)
    {
        log.error("Demo must have capabilities or be played with root perms");
        log.notice(fmt::format("For capabilities, execute linux command: 'sudo setcap cap_net_raw=pe {}'\n", fs::executable_path()));
        if (OS::is_windows)
            time::sleep(5);
        return 1;
    }
    if (argc < 1 || argc > 3)
    {
        log.info("usage: ./demo host [number of pings]");
        if (OS::is_windows)
            time::sleep(5);
        return 1;
    }

    const char *host = argc < 2 ? "google.com" : argv[1];
    unsigned long npings = 10;
    if (argc == 3)
    {
        if (str::to_ulong(argv[2], &npings, 10) == false)
        {
            log.error("Number of pings is not a number");
            if (OS::is_windows)
                time::sleep(5);
            return 1;
        }
        if (npings == 0)
        {
            log.error("No infinite ping allowed");
            if (OS::is_windows)
                time::sleep(5);
            return 1;
        }
    }

    log.notice(fmt::format("Sending {} pings to {}", npings, host));

    OS::add_signal_handler(SIGINT, new Handler<int>([&pinger, &log] (int sig)
    {
        (void)sig;
        log.notice("Stopping ping");
        pinger.stop();
    }));

    log.notice("Press ctrl+C to stop or wait until all pings are done");

    pinger.set_interval(200);
    if (pinger.ping({host, true}, npings) == false)
    {
        log.error(fmt::format("Cannot ping: {}", argv[1]));
        if (OS::is_windows)
            time::sleep(5);
        return 1;
    }

    const auto & result = pinger.result();
    SIHD_COUT(result.str());
    if (OS::is_windows)
        time::sleep(5);
    return 0;
}