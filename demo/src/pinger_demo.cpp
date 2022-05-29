#include <sihd/util/Handler.hpp>
#include <sihd/net/Pinger.hpp>

using namespace sihd::util;
using namespace sihd::net;

Pinger pinger("pinger");

int main(int argc, char **argv)
{
    sihd::util::LoggerManager::console();
    Logger log("demo");

    if (pinger.open(false) == false)
    {
        log.error("Demo must have capabilities or be played with root perms");
        log.notice(Str::format("For capabilities, execute linux command: 'sudo setcap cap_net_raw=pe %s'\n", OS::executable_path().c_str()));
        Time::sleep(5);
        return 1;
    }
    if (argc < 1 || argc > 3)
    {
        log.info("usage: ./demo host [number of pings]");
        Time::sleep(5);
        return 1;
    }
    const char *host = argc < 2 ? "google.com" : argv[1];
    unsigned long npings = 0;
    if (argc == 3)
    {
        if (Str::to_ulong(argv[2], &npings, 10) == false)
        {
            log.error("Number of pings is not a number");
            Time::sleep(5);
            return 1;
        }
    }
    OS::add_signal_handler(SIGINT, new Handler<int>([] (int sig)
    {
        (void)sig;
        pinger.stop();
    }));

    log.notice("Press ctrl+C to get stats or wait until all pings are done");

    pinger.set_interval(200);
    if (pinger.ping({host, true}, npings) == false)
    {
        log.error(Str::format("Cannot ping: %s", argv[1]));
        Time::sleep(5);
        return 1;
    }
    auto result = pinger.result();
    SIHD_COUT(result.str());
    Time::sleep(5);
    return 0;
}