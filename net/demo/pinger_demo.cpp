#include <csignal>

#include <cxxopts.hpp>

#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>

#include <sihd/net/Pinger.hpp>

using namespace sihd::util;
using namespace sihd::net;

int main(int argc, char **argv)
{
    sihd::util::LoggerManager::console();

    cxxopts::Options options(argv[0], "Testing ping of module net");
    options.add_options()("h,help", "Prints usage")(
        "host",
        "Host to ping",
        cxxopts::value<std::string>()->default_value(
            "google.com"))("pings", "Number of pings to send", cxxopts::value<int>()->default_value("10"));

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

    os::add_signal_handler(SIGINT, new Handler<int>([&pinger, &log](int sig) {
                               (void)sig;
                               log.notice("Stopping ping");
                               pinger.stop();
                           }));

    log.notice("Press ctrl+C to stop or wait until all pings are done");

    constexpr bool do_dns_lookup = true;
    constexpr time_t interval_ms = 200;

    pinger.set_interval(interval_ms);
    if (pinger.ping(IpAddr {host, do_dns_lookup}, npings) == false)
    {
        log.error(fmt::format("Cannot ping: {}", argv[1]));
        if (os::is_windows)
            time::sleep(5);
        return 1;
    }

    const auto & result = pinger.result();
    SIHD_COUT(result.str() + "\n");
    if (os::is_windows)
        time::sleep(5);
    return 0;
}
