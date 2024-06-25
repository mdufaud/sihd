#include <csignal>

#include <cxxopts.hpp>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/fs.hpp>

#include <sihd/net/Pinger.hpp>

using namespace sihd::util;
using namespace sihd::net;

int main(int argc, char **argv)
{
    sihd::util::LoggerManager::console();

    cxxopts::Options options(argv[0], "Testing ping of module net");
    // clang-format off
    options.add_options()
        ("h,help", "Prints usage")
        ("host", "Host to ping", cxxopts::value<std::string>()->default_value("google.com"))
        ("pings", "Number of pings to send", cxxopts::value<int>()->default_value("10"));
    // clang-format on
    options.parse_positional({"host", "pings"});

    auto args = options.parse(argc, argv);

    if (args.count("help"))
    {
        fmt::print(options.help());
        return 0;
    }

    Logger log("demo");
    Pinger pinger("pinger");

    if (pinger.open(false) == false)
    {
        log.error("Demo must have capabilities or be played with root perms");
        log.notice(fmt::format("For capabilities, execute linux command: 'sudo setcap cap_net_raw=pe {}'\n",
                               fs::executable_path()));
        if (os::is_windows)
            time::sleep(5);
        return 1;
    }

    const int npings = args["pings"].as<int>();
    const std::string host = args["host"].as<std::string>();

    log.notice(fmt::format("Sending {} pings to {}", npings, host));

    sihd::util::SigWatcher watcher("signal-watcher");

    watcher.add_signal(SIGINT);
    watcher.set_polling_frequency(5);

    Handler<SigWatcher *> sig_handler([&log, &pinger](SigWatcher *watcher) {
        auto & catched_signals = watcher->catched_signals();
        if (catched_signals.empty())
            return;
        log.notice("Stopping ping");
        pinger.stop();
    });
    watcher.add_observer(&sig_handler);
    watcher.start();

    log.notice("Press ctrl+C to stop or wait until all pings are done");

    sihd::util::Handler<Pinger *> ping_handler([&log](Pinger *pinger) {
        const PingEvent & event = pinger->event();

        if (event.sent)
            log.debug("sent ping");
        else if (event.received)
        {
            const IcmpResponse & icmp_response = event.icmp_response;
            log.info(fmt::format("{} bytes from {}: icmp_seq={} ttl={} time={}",
                                 icmp_response.size,
                                 icmp_response.client.host(),
                                 icmp_response.seq,
                                 icmp_response.ttl,
                                 event.trip_time.timeoffset_str()));
        }
        else if (event.timeout)
        {
            log.warning("ping timed out");
        }
    });
    pinger.add_observer(&ping_handler);

    constexpr time_t interval_ms = 200;
    pinger.set_interval(interval_ms);

    constexpr bool do_dns_lookup = true;

    pinger.set_client(IpAddr {host, do_dns_lookup});
    pinger.set_ping_count(npings);

    const bool success = pinger.start();
    pinger.stop();

    if (success)
    {
        const auto & result = pinger.result();
        SIHD_COUT(result.str() + "\n");
    }
    else
        log.error(fmt::format("Cannot ping: {}", argv[1]));

    if constexpr (os::is_windows)
        time::sleep(5);

    return success ? 0 : 1;
}
