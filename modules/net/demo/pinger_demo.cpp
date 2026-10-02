#include <thread>

#include <sihd/net/Pinger.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/macro.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::net;

SIHD_NEW_LOGGER("ping-demo");

int main(int argc, char **argv)
{
    App app({
        .name = "pinger_demo",
        .description = "Testing ping of module net",
    });

    int timeout = 1000;
    int interval = 200;
    std::string host = "google.com";
    int npings = 10;

    app.root().bind("timeout", timeout, "Timeout in ms", "t");
    app.root().bind("interval", interval, "Interval in ms", "i");
    app.root().bind_positional("host", host, "Host to ping");
    app.root().bind_positional("pings", npings, "Number of pings to send");

    Pinger pinger("pinger");

    Handler<Pinger *> ping_handler([](Pinger *pinger) {
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
                     event.trip_time.str());
        }
        else if (event.timeout)
        {
            SIHD_LOG(warning, "ping timed out");
        }
    });
    pinger.add_observer(&ping_handler);

    app.root().on_run([&] {
        if (pinger.open(false).has_value() == false)
        {
            SIHD_LOG(error, "Demo must have capabilities or be played with root perms");
            SIHD_LOG(notice,
                     "For capabilities, execute linux command: 'sudo setcap cap_net_raw=pe {}'\n",
                     fs::executable_path());
            app.exit(EXIT_FAILURE);
        }

        const auto hostaddr_res = dns::find(host);
        if (SIHD_UNEXPECTED_LOG(hostaddr_res))
        {
            SIHD_LOG(error, "Cannot resolve: {}", host);
            app.exit(EXIT_FAILURE);
            return;
        }
        const IpAddr hostaddr = *hostaddr_res;

        SIHD_LOG(notice, "Sending {} pings to {} ({})", npings, host, hostaddr.str());
        SIHD_LOG(notice, "Press ctrl+C to stop or wait until all pings are done");

        SIHD_DIE_FALSE(pinger.set_interval(interval));
        SIHD_DIE_FALSE(pinger.set_timeout(timeout));
        SIHD_DIE_FALSE(pinger.set_client(hostaddr));
        SIHD_DIE_FALSE(pinger.set_ping_count(npings));

        // the blocking service runs aside; its end or a stop signal breaks the loop
        bool success = false;
        std::thread ping_thread([&] {
            success = pinger.start();
            app.stop();
        });
        app.loop();
        pinger.stop();
        ping_thread.join();

        if (success == false)
        {
            SIHD_LOG(error, "Cannot ping: {}", host);
            app.exit(EXIT_FAILURE);
        }
        SIHD_COUT("{}\n", pinger.result().str());
    });

    const int status = app.run(argc, argv);

    if constexpr (build::is_windows)
        time::sleep(5);

    return status;
}
