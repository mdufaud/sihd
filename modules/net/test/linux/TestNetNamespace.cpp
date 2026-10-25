#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <private_dbus.hpp>
#include <span>
#include <string>
#include <thread>
#include <userns_child.hpp>
#include <vector>

#include <gtest/gtest.h>

#include <fmt/format.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/NetMonitor.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/net/route.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include <linux/if_tun.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;
using namespace sihd::net;

namespace
{

using Interfaces = std::map<std::string, NetInterface>;

constexpr int child_ok = 0;
constexpr int child_failed = 1;
constexpr int child_tun_unavailable = 3;
constexpr int child_bridge_unavailable = 4;
constexpr int child_mount_unavailable = 5;
constexpr int child_dbus_unavailable = 6;

constexpr std::array<ns::Type, 1> network_namespace = {ns::Type::net};
constexpr std::array<ns::Type, 2> network_and_mount_namespaces = {ns::Type::net, ns::Type::mount};

const std::map<int, std::string> skipped_child_codes = {
    {child_tun_unavailable, "/dev/net/tun unavailable"},
    {child_bridge_unavailable, "bridge module not loaded"},
    {child_mount_unavailable, "mount(8) missing or refused in the user namespace"},
    {child_dbus_unavailable, "dbus-daemon unavailable"},
};

// the forked child records its gtest failures in its own copy of the test state
int child_result()
{
    return ::testing::Test::HasFailure() ? child_failed : child_ok;
}

bool run_command(std::initializer_list<std::string_view> args)
{
    Process command(args);
    return command.execute() && command.wait_exit().value_or(false) && command.return_code() == 0;
}

// sysfs lists the links of the namespace it was mounted from
bool remount_sysfs()
{
    return run_command({"mount", "-t", "sysfs", "sysfs", "/sys"});
}

bool bind_resolv_conf(const TmpDir & tmp, const std::string & content)
{
    const std::string resolv_conf = tmp.path() + "/resolv.conf";
    return fs::write(resolv_conf, content) && run_command({"mount", "--bind", resolv_conf, "/etc/resolv.conf"});
}

ifreq make_ifreq(const std::string & name)
{
    ifreq request {};
    name.copy(request.ifr_name, IFNAMSIZ - 1);
    return request;
}

Result<Socket> open_control()
{
    Socket control;
    auto opened = control.open(AF_INET, SOCK_DGRAM, 0);
    SIHD_UNEXPECTED_RETURN(opened);
    return control;
}

// a missing or refused /dev/net/tun skips, any other failure is a bug
int tun_failure(const Error & error)
{
    if (error.code == ErrorCode::not_found || error.code == ErrorCode::permission_denied)
        return child_tun_unavailable;
    ADD_FAILURE() << "tun creation failed: " << error.message;
    return child_failed;
}

// a tun device lives while its descriptor is open: the callers keep the File in scope
Result<File> create_tun(const std::string & name, int flags)
{
    File tun;
    auto opened = tun.open("/dev/net/tun", "r+");
    SIHD_UNEXPECTED_RETURN(opened);

    ifreq request = make_ifreq(name);
    request.ifr_flags = static_cast<short>(flags | IFF_NO_PI);
    auto created = os::ioctl(tun.fd(), TUNSETIFF, &request);
    SIHD_UNEXPECTED_RETURN(created);
    return tun;
}

Result<void> set_up(const Socket & control, const std::string & name, bool up)
{
    ifreq request = make_ifreq(name);
    auto flags = os::ioctl(control.socket(), SIOCGIFFLAGS, &request);
    SIHD_UNEXPECTED_RETURN(flags);
    request.ifr_flags = static_cast<short>(up ? request.ifr_flags | IFF_UP : request.ifr_flags & ~IFF_UP);
    return os::ioctl(control.socket(), SIOCSIFFLAGS, &request);
}

Result<void> set_hwaddr(const Socket & control, const std::string & name, const std::array<uint8_t, 6> & mac)
{
    ifreq request = make_ifreq(name);
    request.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    std::memcpy(request.ifr_hwaddr.sa_data, mac.data(), mac.size());
    return os::ioctl(control.socket(), SIOCSIFHWADDR, &request);
}

// a label like "tun0:1" adds a secondary address
Result<void> set_ipv4(const Socket & control, const std::string & label, const std::string & address)
{
    ifreq request = make_ifreq(label);
    const sockaddr_in ip = IpAddr(address).addr4();
    std::memcpy(&request.ifr_addr, &ip, sizeof(ip));
    auto assigned = os::ioctl(control.socket(), SIOCSIFADDR, &request);
    SIHD_UNEXPECTED_RETURN(assigned);
    if (address == "0.0.0.0")
        return {};

    const sockaddr_in netmask = IpAddr("255.255.255.0").addr4();
    std::memcpy(&request.ifr_netmask, &netmask, sizeof(netmask));
    return os::ioctl(control.socket(), SIOCSIFNETMASK, &request);
}

Result<void> add_default_route(const Socket & control, std::string name, uint32_t metric, const std::string & gateway)
{
    rtentry entry {};
    sockaddr_in any {};
    any.sin_family = AF_INET;
    std::memcpy(&entry.rt_dst, &any, sizeof(any));
    std::memcpy(&entry.rt_genmask, &any, sizeof(any));
    entry.rt_flags = RTF_UP;

    if (!gateway.empty())
    {
        const sockaddr_in via = IpAddr(gateway).addr4();
        std::memcpy(&entry.rt_gateway, &via, sizeof(via));
        entry.rt_flags |= RTF_GATEWAY;
    }

    // the kernel stores rt_metric - 1, as route(8) expects
    entry.rt_metric = static_cast<short>(metric + 1);
    entry.rt_dev = name.data();
    return os::ioctl(control.socket(), SIOCADDRT, &entry);
}

const NetInterface *find_interface(const Interfaces & interfaces, const std::string & name)
{
    const auto it = interfaces.find(name);
    return it == interfaces.end() ? nullptr : &it->second;
}

bool has_address(const NetInterface & netif, const std::string & address, const std::string & netmask)
{
    return std::ranges::any_of(netif.addresses(), [&](const NetInterface::Address & entry) {
        return entry.addr.str() == address && entry.netmask.str() == netmask;
    });
}

bool has_event(const std::vector<NetMonitorEvent> & events,
               NetMonitorEventType type,
               bool removed,
               const std::string & interface_name,
               const std::string & address = "")
{
    return std::ranges::any_of(events, [&](const NetMonitorEvent & event) {
        return event.type == type && event.removed == removed && event.interface_name == interface_name
               && (address.empty() || event.address.str() == address);
    });
}

IpAddr scoped_link_local(const std::string & address, const std::string & interface_name)
{
    sockaddr_in6 scoped = IpAddr(address).addr6();
    scoped.sin6_scope_id = if_nametoindex(interface_name.c_str());
    return IpAddr(scoped);
}

int check_virtual_link_kinds()
{
    if (!remount_sysfs())
        return child_mount_unavailable;
    auto tun = create_tun("sihdtun0", IFF_TUN);
    auto tap = create_tun("sihdtap0", IFF_TAP);
    if (!tun)
        return tun_failure(tun.error());
    if (!tap)
        return tun_failure(tap.error());
    if (!fs::exists("/sys/module/bridge"))
        return child_bridge_unavailable;
    auto control = open_control();
    std::string bridge = "sihdbr0";
    if (!control || !os::ioctl(control->socket(), SIOCBRADDBR, bridge.data())
        || !set_hwaddr(*control, "sihdtap0", {0x02, 0, 0, 0, 0, 0x01}) || !set_up(*control, "sihdtun0", true))
        return child_failed;

    auto interfaces = NetInterface::get_all_interfaces();
    EXPECT_TRUE(interfaces.has_value());
    const NetInterface *tun_link = interfaces ? find_interface(*interfaces, "sihdtun0") : nullptr;
    const NetInterface *tap_link = interfaces ? find_interface(*interfaces, "sihdtap0") : nullptr;
    const NetInterface *bridge_link = interfaces ? find_interface(*interfaces, "sihdbr0") : nullptr;
    if (tun_link == nullptr || tap_link == nullptr || bridge_link == nullptr)
    {
        ADD_FAILURE() << "a created link is missing";
        return child_failed;
    }

    EXPECT_EQ(tun_link->kind(), NetInterface::Kind::tunnel);
    EXPECT_EQ(tap_link->kind(), NetInterface::Kind::tunnel);
    EXPECT_EQ(bridge_link->kind(), NetInterface::Kind::bridge);
    EXPECT_FALSE(tun_link->hardware());
    EXPECT_FALSE(tap_link->hardware());
    EXPECT_FALSE(bridge_link->hardware());
    EXPECT_EQ(tun_link->mtu(), 1500u);
    // the tun driver speed changed across kernel versions
    const auto speed_mbps = fs::read_all("/sys/class/net/sihdtun0/speed");
    EXPECT_TRUE(speed_mbps.has_value());
    EXPECT_EQ(tun_link->link_speed_bps(), std::stoull(speed_mbps.value_or("0")) * 1'000'000);
    EXPECT_GT(tun_link->link_speed_bps(), 0u);
    EXPECT_EQ(tap_link->mac_addr(), "02:00:00:00:00:01");
    EXPECT_GT(tun_link->index(), 0u);
    EXPECT_NE(tun_link->index(), tap_link->index());
    return child_result();
}

int check_every_address_is_listed()
{
    auto tun = create_tun("sihdtun0", IFF_TUN);
    auto tap = create_tun("sihdtap0", IFF_TAP);
    if (!tun)
        return tun_failure(tun.error());
    if (!tap)
        return tun_failure(tap.error());
    auto control = open_control();
    if (!control || !set_ipv4(*control, "sihdtun0", "10.10.0.1") || !set_ipv4(*control, "sihdtun0:1", "10.10.1.1")
        || !set_ipv4(*control, "sihdtap0", "10.20.0.1"))
        return child_failed;

    auto interfaces = NetInterface::get_all_interfaces();
    const NetInterface *tun_link = interfaces ? find_interface(*interfaces, "sihdtun0") : nullptr;
    const NetInterface *tap_link = interfaces ? find_interface(*interfaces, "sihdtap0") : nullptr;
    if (tun_link == nullptr || tap_link == nullptr)
    {
        ADD_FAILURE() << "a created link is missing";
        return child_failed;
    }

    EXPECT_TRUE(has_address(*tun_link, "10.10.0.1", "255.255.255.0"));
    EXPECT_TRUE(has_address(*tun_link, "10.10.1.1", "255.255.255.0"));
    EXPECT_EQ(tun_link->addr4().str(), "10.10.0.1");
    EXPECT_EQ(tap_link->extra_addr4().str(), "10.20.0.255");
    EXPECT_FALSE(interfaces->contains("sihdtun0:1"));
    return child_result();
}

int check_lowest_metric_route_wins()
{
    auto tun = create_tun("sihdtun0", IFF_TUN);
    auto tap = create_tun("sihdtap0", IFF_TAP);
    if (!tun)
        return tun_failure(tun.error());
    if (!tap)
        return tun_failure(tap.error());
    auto control = open_control();
    if (!control || !set_ipv4(*control, "sihdtun0", "10.10.0.1") || !set_up(*control, "sihdtun0", true)
        || !set_ipv4(*control, "sihdtap0", "10.20.0.1") || !set_up(*control, "sihdtap0", true)
        || !add_default_route(*control, "sihdtun0", 100, "")
        || !add_default_route(*control, "sihdtap0", 50, "10.20.0.254"))
        return child_failed;

    auto found = route::find(IpAddr("192.0.2.1"));
    auto defaults = route::defaults_ipv4();
    if (!found || !defaults || defaults->size() != 2)
    {
        ADD_FAILURE() << "lookups failed or the default routes are not the two added";
        return child_failed;
    }

    EXPECT_EQ(found->interface_name, "sihdtap0");
    EXPECT_EQ(found->gateway.str(), "10.20.0.254");
    EXPECT_EQ(found->source.str(), "10.20.0.1");
    EXPECT_EQ(defaults->at(0).interface_name, "sihdtap0");
    EXPECT_EQ(defaults->at(0).gateway.str(), "10.20.0.254");
    EXPECT_EQ(defaults->at(0).metric, 50u);
    EXPECT_EQ(defaults->at(1).interface_name, "sihdtun0");
    EXPECT_FALSE(defaults->at(1).gateway.has_ip());
    EXPECT_EQ(defaults->at(1).metric, 100u);
    return child_result();
}

int check_scoped_link_local_route()
{
    auto first = create_tun("sihdtap0", IFF_TAP);
    auto second = create_tun("sihdtap1", IFF_TAP);
    if (!first)
        return tun_failure(first.error());
    if (!second)
        return tun_failure(second.error());
    auto control = open_control();
    if (!control || !set_up(*control, "sihdtap0", true) || !set_up(*control, "sihdtap1", true))
        return child_failed;

    auto through_first = route::find(scoped_link_local("fe80::1", "sihdtap0"));
    auto through_second = route::find(scoped_link_local("fe80::1", "sihdtap1"));
    if (!through_first || !through_second)
    {
        ADD_FAILURE() << "link-local lookups failed";
        return child_failed;
    }

    EXPECT_EQ(through_first->interface_name, "sihdtap0");
    EXPECT_EQ(through_second->interface_name, "sihdtap1");
    return child_result();
}

int check_resolv_conf_servers()
{
    auto tun = create_tun("sihdtun0", IFF_TUN);
    if (!tun)
        return tun_failure(tun.error());
    TmpDir tmp;
    if (!bind_resolv_conf(tmp, "# generated\nnameserver 1.2.3.4\nnameserver fe80::1%sihdtun0\nsearch example.org\n"))
        return child_mount_unavailable;

    auto servers = dns::servers();
    if (!servers || servers->size() != 2)
    {
        ADD_FAILURE() << "expected the two resolv.conf servers";
        return child_failed;
    }

    EXPECT_EQ(servers->at(0).addr.str(), "1.2.3.4");
    EXPECT_EQ(servers->at(0).interface_name, "");
    EXPECT_EQ(servers->at(1).addr.str(), "fe80::1");
    EXPECT_EQ(servers->at(1).interface_name, "sihdtun0");
    EXPECT_EQ(servers->at(1).addr.addr6().sin6_scope_id, if_nametoindex("sihdtun0"));
    return child_result();
}

int check_resolved_stub_without_resolved()
{
    TmpDir tmp;
    if (!bind_resolv_conf(tmp, "nameserver 127.0.0.53\n"))
        return child_mount_unavailable;
    if (!env::set("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=" + tmp.path() + "/no_bus"))
        return child_failed;

    auto servers = dns::servers();
    if (!servers || servers->size() != 1)
    {
        ADD_FAILURE() << "expected the resolved stub alone";
        return child_failed;
    }

    EXPECT_EQ(servers->at(0).addr.str(), "127.0.0.53");
    EXPECT_EQ(servers->at(0).interface_name, "");
    return child_result();
}

Result<void> append_dns_entry(dbus::Appender & entries, int32_t index, int32_t family, const std::vector<uint8_t> & bytes)
{
    auto entry = entries.open_struct();
    SIHD_UNEXPECTED_RETURN(entry);
    auto index_added = entry->add<int32_t>(index);
    SIHD_UNEXPECTED_RETURN(index_added);
    auto family_added = entry->add<int32_t>(family);
    SIHD_UNEXPECTED_RETURN(family_added);

    auto address = entry->open_array("y");
    SIHD_UNEXPECTED_RETURN(address);
    for (const uint8_t byte : bytes)
    {
        auto byte_added = address->add<uint8_t>(byte);
        SIHD_UNEXPECTED_RETURN(byte_added);
    }

    auto address_closed = entry->close(std::move(*address));
    SIHD_UNEXPECTED_RETURN(address_closed);
    return entries.close(std::move(*entry));
}

Result<void> answer_resolved_dns(dbus::Connection & server, int32_t link_index)
{
    auto call = receive_member(server, "Get");
    if (!call)
        return std::unexpected(Error(ErrorCode::timeout, "no Get call"));
    auto reply = call->method_return();
    SIHD_UNEXPECTED_RETURN(reply);

    dbus::Appender body(*reply);
    auto variant = body.open_variant("a(iiay)");
    SIHD_UNEXPECTED_RETURN(variant);
    auto entries = variant->open_array("(iiay)");
    SIHD_UNEXPECTED_RETURN(entries);
    auto global_entry = append_dns_entry(*entries, 0, AF_INET, {9, 9, 9, 9});
    SIHD_UNEXPECTED_RETURN(global_entry);
    auto link_entry = append_dns_entry(*entries,
                                       link_index,
                                       AF_INET6,
                                       {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2});
    SIHD_UNEXPECTED_RETURN(link_entry);
    auto entries_closed = variant->close(std::move(*entries));
    SIHD_UNEXPECTED_RETURN(entries_closed);
    auto variant_closed = body.close(std::move(*variant));
    SIHD_UNEXPECTED_RETURN(variant_closed);
    return reply->send(server);
}

int check_resolved_servers()
{
    PrivateDBus bus;
    if (!PrivateDBus::available() || !bus.start())
        return child_dbus_unavailable;
    auto tun = create_tun("sihdtun0", IFF_TUN);
    if (!tun)
        return tun_failure(tun.error());
    TmpDir tmp;
    if (!bind_resolv_conf(tmp, "nameserver 127.0.0.53\noptions edns0 trust-ad\n"))
        return child_mount_unavailable;

    dbus::Connection server;
    if (!env::set("DBUS_SYSTEM_BUS_ADDRESS", bus.address()) || !server.open(bus.address())
        || !server.request_name("org.freedesktop.resolve1"))
        return child_failed;

    Result<std::vector<dns::DnsServer>> servers;
    {
        std::jthread client([&servers] { servers = dns::servers(); });
        EXPECT_TRUE(answer_resolved_dns(server, static_cast<int32_t>(if_nametoindex("sihdtun0"))).has_value());
    }
    if (!servers || servers->size() != 2)
    {
        ADD_FAILURE() << "expected the two resolved servers";
        return child_failed;
    }

    EXPECT_EQ(servers->at(0).addr.str(), "9.9.9.9");
    EXPECT_EQ(servers->at(0).interface_name, "");
    EXPECT_EQ(servers->at(1).addr.str(), "fe80::2");
    EXPECT_EQ(servers->at(1).interface_name, "sihdtun0");
    return child_result();
}

// the kernel queues the notifications on the socket: run() drains them on this same thread
int check_monitor_events()
{
    auto tun = create_tun("sihdtun0", IFF_TUN);
    if (!tun)
        return tun_failure(tun.error());
    auto control = open_control();
    if (!control)
        return child_failed;

    NetMonitor monitor(std::chrono::milliseconds(100));
    size_t notifications = 0;
    Handler<NetMonitor *> counter([&notifications](NetMonitor *) { ++notifications; });
    monitor.add_observer(&counter);

    if (!set_ipv4(*control, "sihdtun0", "10.10.0.1") || !set_ipv4(*control, "sihdtun0:1", "10.10.1.1")
        || !set_up(*control, "sihdtun0", true))
        return child_failed;
    EXPECT_TRUE(monitor.run());
    const std::vector<NetMonitorEvent> configure_events = monitor.events();

    if (!set_ipv4(*control, "sihdtun0", "0.0.0.0") || !set_up(*control, "sihdtun0", false))
        return child_failed;
    EXPECT_TRUE(monitor.run());
    const std::vector<NetMonitorEvent> unconfigure_events = monitor.events();

    *tun = File();
    EXPECT_TRUE(monitor.run());
    const std::vector<NetMonitorEvent> delete_events = monitor.events();

    EXPECT_TRUE(monitor.run());

    EXPECT_TRUE(has_event(configure_events, NetMonitorEventType::address, false, "sihdtun0", "10.10.0.1"));
    EXPECT_TRUE(has_event(configure_events, NetMonitorEventType::address, false, "sihdtun0", "10.10.1.1"));
    EXPECT_TRUE(has_event(configure_events, NetMonitorEventType::route, false, "sihdtun0"));
    EXPECT_TRUE(has_event(configure_events, NetMonitorEventType::link, false, "sihdtun0"));
    EXPECT_TRUE(has_event(unconfigure_events, NetMonitorEventType::address, true, "sihdtun0", "10.10.0.1"));
    EXPECT_TRUE(has_event(unconfigure_events, NetMonitorEventType::link, false, "sihdtun0"));
    EXPECT_TRUE(has_event(delete_events, NetMonitorEventType::link, true, "sihdtun0"));
    EXPECT_TRUE(has_event(delete_events, NetMonitorEventType::address, true, "sihdtun0", "10.10.1.1"));
    EXPECT_TRUE(monitor.events().empty());
    EXPECT_EQ(notifications, 3u);
    return child_result();
}

// more changes than the socket buffer holds before run() reads any of them
int check_monitor_overflow()
{
    auto tun = create_tun("sihdtun0", IFF_TUN);
    if (!tun)
        return tun_failure(tun.error());
    auto control = open_control();
    if (!control || !set_up(*control, "sihdtun0", true))
        return child_failed;

    NetMonitor monitor(std::chrono::milliseconds(100));
    for (int alias = 0; alias < 500; ++alias)
    {
        const std::string address = fmt::format("10.{}.{}.1", 100 + alias / 250, alias % 250);
        if (!set_ipv4(*control, fmt::format("sihdtun0:{}", alias), address))
            return child_failed;
    }
    EXPECT_TRUE(monitor.run());

    EXPECT_TRUE(std::ranges::any_of(monitor.events(), [](const NetMonitorEvent & event) {
        return event.type == NetMonitorEventType::overflow;
    }));
    return child_result();
}

int check_unreachable_destination()
{
    auto found = route::find(IpAddr("192.0.2.1"));
    auto defaults = route::defaults_ipv4();
    if (found || !defaults)
    {
        ADD_FAILURE() << "an empty namespace routes nowhere";
        return child_failed;
    }

    EXPECT_EQ(found.error().code, ErrorCode::not_found);
    EXPECT_TRUE(defaults->empty());
    return child_result();
}

} // namespace

class TestNetNamespace: public ::testing::Test
{
    protected:
        TestNetNamespace() { LoggerManager::stream(); }

        virtual ~TestNetNamespace() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNetNamespace, test_virtual_link_kinds)
{
    expect_in_userns(network_and_mount_namespaces, check_virtual_link_kinds, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_every_address_is_listed)
{
    expect_in_userns(network_namespace, check_every_address_is_listed, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_lowest_metric_route_wins)
{
    expect_in_userns(network_namespace, check_lowest_metric_route_wins, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_scoped_link_local_route)
{
    expect_in_userns(network_namespace, check_scoped_link_local_route, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_resolv_conf_servers)
{
    expect_in_userns(network_and_mount_namespaces, check_resolv_conf_servers, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_resolved_stub_without_resolved)
{
    expect_in_userns(network_and_mount_namespaces, check_resolved_stub_without_resolved, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_resolved_servers)
{
    if constexpr (build::is_run_with_tsan)
        GTEST_SKIP_("tsan kills a thread started in the forked child");

    expect_in_userns(network_and_mount_namespaces, check_resolved_servers, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_monitor_events)
{
    expect_in_userns(network_namespace, check_monitor_events, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_monitor_overflow)
{
    expect_in_userns(network_namespace, check_monitor_overflow, skipped_child_codes);
}

TEST_F(TestNetNamespace, test_unreachable_destination)
{
    expect_in_userns(network_namespace, check_unreachable_destination, skipped_child_codes);
}

} // namespace test
