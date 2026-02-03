#include <csignal>

#include <CLI/CLI.hpp>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/macro.hpp>

#include <sihd/net/Pinger.hpp>
#include <sihd/net/dns.hpp>

using namespace sihd::util;
using namespace sihd::net;

SIHD_NEW_LOGGER("ping-demo");

int main(int argc, char **argv)
{
    sihd::util::LoggerManager::console();

    int timeout = 1000;
    int interval = 200;
    std::string host = "google.com";
    int npings = 10;

    CLI::App app {"Testing ping of module net"};
    app.add_option("-t,--timeout", timeout, "Timeout in ms")->default_val("1000");
    app.add_option("-i,--interval", interval, "Interval in ms")->default_val("200");
    app.add_option("host", host, "Host to ping")->default_val("google.com");
    app.add_option("pings", npings, "Number of pings to send")->default_val("10");

    CLI11_PARSE(app, argc, argv);

    Pinger pinger("pinger");

    if (pinger.open(false) == false)
    {
        SIHD_LOG(error, "Demo must have capabilities or be played with root perms");
        SIHD_LOG(notice,
                 "For capabilities, execute linux command: 'sudo setcap cap_net_raw=pe {}'\n",
                 fs::executable_path());
        if (os::is_windows)
            time::sleep(5);
        return EXIT_FAILURE;
    }

    const IpAddr hostaddr = dns::find(host);
    if (hostaddr.empty())
    {
        return EXIT_FAILURE;
    }

    SIHD_LOG(notice, "Sending {} pings to {} ({})", npings, host, hostaddr.str());

    SigWatcher watcher({SIGINT}, [&pinger]([[maybe_unused]] int sig) {
        SIHD_LOG(notice, "Stopping ping");
        pinger.stop();
    });

    SIHD_LOG(notice, "Press ctrl+C to stop or wait until all pings are done");

    sihd::util::Handler<Pinger *> ping_handler([](Pinger *pinger) {
        const PingEvent & event = pinger->event();

        if (event.sent)
        {
            SIHD_LOG(debug, "sent ping");
        }
        else if (event.received)
        {
            const IcmpResponse & icmp_response = event.icmp_response;
            SIHD_LOG(info,
                     "{} bytes from {}: icmp_seq={} ttl={} time={}",
                     icmp_response.size,
                     icmp_response.client.hostname(),
                     icmp_response.seq,
                     icmp_response.ttl,
                     event.trip_time.timeoffset_str());
        }
        else if (event.timeout)
        {
            SIHD_LOG(warning, "ping timed out");
        }
    });
    pinger.add_observer(&ping_handler);

    SIHD_DIE_FALSE(pinger.set_interval(interval));
    SIHD_DIE_FALSE(pinger.set_timeout(timeout));
    SIHD_DIE_FALSE(pinger.set_client(hostaddr));
    SIHD_DIE_FALSE(pinger.set_ping_count(npings));

    const bool success = pinger.start();
    pinger.stop();

    if (success)
    {
        const auto & result = pinger.result();
        SIHD_COUT("{}\n", result.str());
    }
    else
        SIHD_LOG(error, "Cannot ping: {}", host);

    if constexpr (os::is_windows)
        time::sleep(5);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
