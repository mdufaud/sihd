#include <string>

// clang-format off
#include <sihd/lua/net/LuaNetApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/checked.hpp>
// clang-format on

#include <sihd/core/Device.hpp>
#include <sihd/net/DeviceTcpClient.hpp>
#include <sihd/net/DeviceTcpServer.hpp>
#include <sihd/net/DeviceUdpReceiver.hpp>
#include <sihd/net/DeviceUdpSender.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/NetMonitor.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/net/route.hpp>
#include <sihd/net/wifi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace sihd::lua
{

using namespace sihd::net;
using sihd::core::Device;
using namespace sihd::util;

SIHD_LOGGER;

namespace
{

// luabridge copies the values it returns: the copies share one monitor
class LuaNetMonitor
{
    public:
        static Result<LuaNetMonitor> open(luabridge::LuaRef run_timeout)
        {
            LuaNetMonitor monitor;
            monitor._monitor = std::make_shared<NetMonitor>();
            if (!run_timeout.isNil())
                monitor._monitor->set_run_timeout(to_duration(run_timeout));
            return monitor;
        }

        void set_run_timeout(luabridge::LuaRef timeout) { _monitor->set_run_timeout(to_duration(timeout)); }
        bool run(lua_State *state)
        {
            LuaGilRelease release(state);
            return _monitor->run();
        }
        std::vector<NetMonitorEvent> events() const { return _monitor->events(); }

    private:
        std::shared_ptr<NetMonitor> _monitor;
};

} // namespace

void LuaNetApi::load_all(Vm & vm)
{
    LuaNetApi::load_base(vm);
}

void LuaNetApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("net")
        .beginClass<IpAddr>("IpAddr")
        .addConstructor<void (*)()>()
        .addFunction(
            "set_hostname",
            +[](IpAddr *self, const std::string & hostname) { self->set_hostname(hostname); })
        .addFunction("empty", &IpAddr::empty)
        .addFunction("has_ip", &IpAddr::has_ip)
        .addFunction("is_ipv4", &IpAddr::is_ipv4)
        .addFunction("is_ipv6", &IpAddr::is_ipv6)
        .addFunction("port", &IpAddr::port)
        .addFunction("fetch_hostname", checked<+[](lua_State *state, IpAddr *self) {
                         LuaGilRelease release(state);
                         return self->fetch_hostname();
                     }>)
        .addFunction(
            "hostname",
            +[](IpAddr *self) -> std::string { return self->hostname(); })
        .addFunction("str", &IpAddr::str)
        .addFunction("set_subnet_mask",
                     checked<+[](IpAddr *self, const std::string & mask) { return self->set_subnet_mask(mask); }>)
        .addFunction("has_subnet", &IpAddr::has_subnet)
        .addFunction("subnet_value", &IpAddr::subnet_value)
        .addFunction("dump_subnet", &IpAddr::dump_subnet)
        .addFunction(
            "is_same_subnet",
            +[](IpAddr *self, const IpAddr & other) -> bool { return self->is_same_subnet(other); })
        .endClass()
        .addFunction(
            "ip_addr",
            +[](const std::string & host, luabridge::LuaRef port) -> IpAddr {
                if (port.isNil())
                    return IpAddr(host);
                return IpAddr(host, static_cast<int>(port));
            })
        .beginClass<NetInterface::Address>("NetInterfaceAddress")
        .addProperty("addr", &NetInterface::Address::addr, false)
        .addProperty("netmask", &NetInterface::Address::netmask, false)
        .addProperty("extra_addr", &NetInterface::Address::extra_addr, false)
        .endClass()
        .beginClass<NetInterface>("NetInterface")
        .addStaticProperty("Kind",
                           [](lua_State *state) {
                               luabridge::LuaRef kinds = luabridge::newTable(state);
                               kinds["unknown"] = NetInterface::Kind::unknown;
                               kinds["loopback"] = NetInterface::Kind::loopback;
                               kinds["ethernet"] = NetInterface::Kind::ethernet;
                               kinds["wifi"] = NetInterface::Kind::wifi;
                               kinds["cellular"] = NetInterface::Kind::cellular;
                               kinds["tunnel"] = NetInterface::Kind::tunnel;
                               kinds["ppp"] = NetInterface::Kind::ppp;
                               kinds["bridge"] = NetInterface::Kind::bridge;
                               return kinds;
                           })
        .addStaticFunction("get_all_interfaces", checked<+[](lua_State *state) {
                               LuaGilRelease release(state);
                               return NetInterface::get_all_interfaces();
                           }>)
        .addStaticFunction(
            "kind_str",
            +[](NetInterface::Kind kind) { return std::string(NetInterface::kind_str(kind)); })
        .addFunction(
            "name",
            +[](const NetInterface *self) { return self->name(); })
        .addFunction(
            "mac_addr",
            +[](const NetInterface *self) { return self->mac_addr(); })
        .addFunction("index", &NetInterface::index)
        .addFunction("kind", &NetInterface::kind)
        .addFunction("hardware", &NetInterface::hardware)
        .addFunction("mtu", &NetInterface::mtu)
        .addFunction("link_speed_bps", &NetInterface::link_speed_bps)
        .addFunction(
            "addresses",
            +[](const NetInterface *self) { return self->addresses(); })
        .addFunction(
            "addr4",
            +[](const NetInterface *self) { return self->addr4(); })
        .addFunction(
            "netmask4",
            +[](const NetInterface *self) { return self->netmask4(); })
        .addFunction(
            "extra_addr4",
            +[](const NetInterface *self) { return self->extra_addr4(); })
        .addFunction(
            "addr6",
            +[](const NetInterface *self) { return self->addr6(); })
        .addFunction(
            "netmask6",
            +[](const NetInterface *self) { return self->netmask6(); })
        .addFunction(
            "extra_addr6",
            +[](const NetInterface *self) { return self->extra_addr6(); })
        .addFunction("up", &NetInterface::up)
        .addFunction("broadcast", &NetInterface::broadcast)
        .addFunction("loopback", &NetInterface::loopback)
        .addFunction("point2point", &NetInterface::point2point)
        .addFunction("running", &NetInterface::running)
        .addFunction("noarp", &NetInterface::noarp)
        .addFunction("promisc", &NetInterface::promisc)
        .addFunction("notrailers", &NetInterface::notrailers)
        .addFunction("master", &NetInterface::master)
        .addFunction("slave", &NetInterface::slave)
        .addFunction("all_multicast", &NetInterface::all_multicast)
        .addFunction("supports_multicast", &NetInterface::supports_multicast)
        .endClass()
        .beginNamespace("route")
        .beginClass<route::Route>("Route")
        .addProperty("interface_name", &route::Route::interface_name, false)
        .addProperty("gateway", &route::Route::gateway, false)
        .addProperty("source", &route::Route::source, false)
        .addProperty("metric", &route::Route::metric, false)
        .endClass()
        .addFunction("find", checked<+[](lua_State *state, const IpAddr & destination) {
                         LuaGilRelease release(state);
                         return route::find(destination);
                     }>)
        .addFunction("defaults_ipv4", checked<+[](lua_State *state) {
                         LuaGilRelease release(state);
                         return route::defaults_ipv4();
                     }>)
        .addFunction("defaults_ipv6", checked<+[](lua_State *state) {
                         LuaGilRelease release(state);
                         return route::defaults_ipv6();
                     }>)
        .endNamespace()
        .beginNamespace("dns")
        .beginClass<dns::DnsServer>("DnsServer")
        .addProperty("addr", &dns::DnsServer::addr, false)
        .addProperty("interface_name", &dns::DnsServer::interface_name, false)
        .endClass()
        .addFunction("servers", checked<+[](lua_State *state, luabridge::LuaRef resolver_timeout) {
                         const Duration timeout = resolver_timeout.isNil() ? dns::default_resolver_timeout
                                                                           : to_duration(resolver_timeout);
                         LuaGilRelease release(state);
                         return dns::servers(timeout);
                     }>)
        .endNamespace()
        .beginNamespace("wifi")
        .beginClass<wifi::Link>("Link")
        .addProperty("ssid", &wifi::Link::ssid, false)
        .addProperty("bssid", &wifi::Link::bssid, false)
        .addProperty("signal_dbm", &wifi::Link::signal_dbm, false)
        .addProperty("frequency_mhz", &wifi::Link::frequency_mhz, false)
        .addProperty("tx_bitrate_bps", &wifi::Link::tx_bitrate_bps, false)
        .endClass()
        .addFunction("link", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::link(interface_name);
                     }>)
        .endNamespace()
        .beginNamespace("NetMonitorEventType")
        .addVariable("unknown", NetMonitorEventType::unknown)
        .addVariable("link", NetMonitorEventType::link)
        .addVariable("address", NetMonitorEventType::address)
        .addVariable("route", NetMonitorEventType::route)
        .addVariable("overflow", NetMonitorEventType::overflow)
        .endNamespace()
        .beginClass<NetMonitorEvent>("NetMonitorEvent")
        .addConstructor<void (*)()>()
        .addProperty("type", &NetMonitorEvent::type, false)
        .addProperty("removed", &NetMonitorEvent::removed, false)
        .addProperty("interface_name", &NetMonitorEvent::interface_name, false)
        .addProperty("address", &NetMonitorEvent::address, false)
        .addFunction("type_str", &NetMonitorEvent::type_str)
        .endClass()
        .beginClass<LuaNetMonitor>("NetMonitor")
        .addStaticFunction("open", checked<&LuaNetMonitor::open>)
        .addFunction("set_run_timeout", &LuaNetMonitor::set_run_timeout)
        .addFunction("run", &LuaNetMonitor::run)
        .addFunction("events", &LuaNetMonitor::events)
        .endClass()
        // devices (derive from core Device: core must be loaded first)
        .deriveClass<DeviceTcpClient, Device>("DeviceTcpClient")
        .addConstructorFrom<SmartNodePtr<DeviceTcpClient>, void(const std::string &, Node *)>()
        .addFunction("set_host", &DeviceTcpClient::set_host)
        .addFunction("set_port", &DeviceTcpClient::set_port)
        .addFunction("set_unix_path", &DeviceTcpClient::set_unix_path)
        .addFunction("set_buffer_capacity", &DeviceTcpClient::set_buffer_capacity)
        .addFunction("set_poll_timeout", &DeviceTcpClient::set_poll_timeout)
        .addFunction("set_connect_timeout", &DeviceTcpClient::set_connect_timeout)
        .addFunction("set_reconnect_interval", &DeviceTcpClient::set_reconnect_interval)
        .endClass()
        .deriveClass<DeviceTcpServer, Device>("DeviceTcpServer")
        .addConstructorFrom<SmartNodePtr<DeviceTcpServer>, void(const std::string &, Node *)>()
        .addFunction("set_host", &DeviceTcpServer::set_host)
        .addFunction("set_port", &DeviceTcpServer::set_port)
        .addFunction("set_unix_path", &DeviceTcpServer::set_unix_path)
        .addFunction("set_buffer_capacity", &DeviceTcpServer::set_buffer_capacity)
        .addFunction("set_poll_timeout", &DeviceTcpServer::set_poll_timeout)
        .addFunction("set_poll_limit", &DeviceTcpServer::set_poll_limit)
        .addFunction("set_queue_size", &DeviceTcpServer::set_queue_size)
        .addFunction("set_max_clients", &DeviceTcpServer::set_max_clients)
        .endClass()
        .deriveClass<DeviceUdpSender, Device>("DeviceUdpSender")
        .addConstructorFrom<SmartNodePtr<DeviceUdpSender>, void(const std::string &, Node *)>()
        .addFunction("set_host", &DeviceUdpSender::set_host)
        .addFunction("set_port", &DeviceUdpSender::set_port)
        .addFunction("set_unix_path", &DeviceUdpSender::set_unix_path)
        .endClass()
        .deriveClass<DeviceUdpReceiver, Device>("DeviceUdpReceiver")
        .addConstructorFrom<SmartNodePtr<DeviceUdpReceiver>, void(const std::string &, Node *)>()
        .addFunction("set_host", &DeviceUdpReceiver::set_host)
        .addFunction("set_port", &DeviceUdpReceiver::set_port)
        .addFunction("set_buffer_capacity", &DeviceUdpReceiver::set_buffer_capacity)
        .addFunction("set_poll_timeout", &DeviceUdpReceiver::set_poll_timeout)
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
