#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <sihd/net/NetInterface.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi.hpp>

using namespace sihd::util;
using namespace sihd::wifi;

namespace
{

const Duration scan_timeout(time::sec(15));

std::vector<std::string> wifi_interfaces()
{
    std::vector<std::string> names;
    auto interfaces = sihd::net::NetInterface::get_all_interfaces();
    if (!interfaces)
    {
        fmt::print(stderr, "{}\n", interfaces.error().message);
        return names;
    }
    for (const auto & [name, netif] : *interfaces)
    {
        if (netif.kind() == sihd::net::NetInterface::Kind::wifi)
            names.emplace_back(name);
    }
    return names;
}

void print_interface(const std::string & interface_name)
{
    fmt::print("{}\n", interface_name);
    auto managing = backend(interface_name);
    auto current = state(interface_name);
    fmt::print("  backend: {}\n", managing ? backend_str(*managing) : managing.error().message);
    fmt::print("  state: {}\n", current ? state_str(*current) : current.error().message);
    auto associated = link(interface_name);
    if (associated)
        fmt::print("  link: '{}' {} {} dBm {} MHz\n",
                   associated->ssid,
                   associated->bssid,
                   associated->signal_dbm,
                   associated->frequency_mhz);

    auto heard = access_points(interface_name);
    if (!heard)
    {
        fmt::print("  access points: {}\n", heard.error().message);
        return;
    }
    for (const AccessPoint & access_point : *heard)
    {
        fmt::print("  {} {:>4} dBm {:>4} MHz {:<10} {:>6} ms {}'{}'\n",
                   access_point.bssid,
                   access_point.signal_dbm,
                   access_point.frequency_mhz,
                   security_str(access_point.security),
                   access_point.seen_ago.milliseconds(),
                   access_point.associated ? "* " : "",
                   access_point.ssid);
    }
}

void print_known(const std::string & interface_name)
{
    auto known = known_networks(interface_name);
    if (!known)
    {
        fmt::print(stderr, "{}: {}\n", interface_name, known.error().message);
        return;
    }
    for (const KnownNetwork & network : *known)
        fmt::print("{}: '{}' {}\n", interface_name, network.ssid, security_str(network.security));
}

void watch(sihd::sys::App & app, const std::string & interface_name)
{
    Monitor monitor(std::chrono::milliseconds(500));
    if (auto opened = monitor.open(interface_name); !opened)
    {
        fmt::print(stderr, "{}: {}\n", interface_name, opened.error().message);
        app.exit(EXIT_FAILURE);
        return;
    }
    Handler<Monitor *> printer([&interface_name](Monitor *changed) {
        for (const Monitor::Event & event : changed->events())
        {
            if (event.type == Monitor::Event::Type::scan_done)
                fmt::print("{}: scan done\n", interface_name);
            else
                fmt::print("{}: {}\n", interface_name, state_str(event.state));
        }
    });
    monitor.add_observer(&printer);

    bool failed = false;
    app.loop(std::nullopt, [&monitor, &failed] {
        failed = !monitor.run();
        return !failed;
    });
    if (failed)
        app.exit(EXIT_FAILURE);
}

} // namespace

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "wifi_demo",
        .description = "List wifi interfaces, their backend, state and the access points heard lately",
    });

    app.root().on_run([] {
        for (const std::string & interface_name : wifi_interfaces())
            print_interface(interface_name);
    });

    app.root().add_command("scan", "ask every wifi backend for a scan, then list").on_run([] {
        for (const std::string & interface_name : wifi_interfaces())
        {
            auto scanned = scan(interface_name, scan_timeout);
            if (!scanned)
                fmt::print(stderr, "{}: {}\n", interface_name, scanned.error().message);
            print_interface(interface_name);
        }
    });

    app.root().add_command("known", "list the networks the backends remember").on_run([] {
        for (const std::string & interface_name : wifi_interfaces())
            print_known(interface_name);
    });

    app.root().add_command("watch", "print the first wifi interface events until interrupted").on_run([&app] {
        const std::vector<std::string> interfaces = wifi_interfaces();
        if (interfaces.empty())
        {
            fmt::print(stderr, "no wifi interface\n");
            app.exit(EXIT_FAILURE);
            return;
        }
        watch(app, interfaces.front());
    });

    return app.run(argc, argv);
}
