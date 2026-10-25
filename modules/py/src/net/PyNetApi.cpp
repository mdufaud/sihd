#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <optional>

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
#include <sihd/py/net/PyNetApi.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include "../expected_caster.hpp"

namespace sihd::py
{

using namespace sihd::util;
using namespace sihd::net;
using sihd::core::Device;

SIHD_LOGGER;

namespace
{

std::unique_ptr<NetMonitor> make_net_monitor(std::optional<Duration> run_timeout)
{
    try
    {
        auto monitor = std::make_unique<NetMonitor>();
        if (run_timeout)
            monitor->set_run_timeout(*run_timeout);
        return monitor;
    }
    catch (const std::runtime_error & error)
    {
        raise_error(Error(ErrorCode::unknown, error.what()));
    }
}

} // namespace

void PyNetApi::add_net_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_core") == false)
        return;

    pybind11::module m_net = m_sihd.def_submodule("net", "sihd::net");

    pybind11::class_<IpAddr>(m_net, "IpAddr")
        .def(pybind11::init<>())
        .def(pybind11::init<std::string_view>())
        .def(pybind11::init<std::string_view, int>())
        .def(pybind11::init<int, bool>(), pybind11::arg("port"), pybind11::arg("ipv6") = false)
        .def_static("localhost", &IpAddr::localhost, pybind11::arg("port"), pybind11::arg("ipv6") = false)
        .def("set_hostname", &IpAddr::set_hostname)
        .def("empty", &IpAddr::empty)
        .def("has_ip", &IpAddr::has_ip)
        .def("is_ipv4", &IpAddr::is_ipv4)
        .def("is_ipv6", &IpAddr::is_ipv6)
        .def("port", &IpAddr::port)
        .def("fetch_hostname", &IpAddr::fetch_hostname)
        .def("hostname", &IpAddr::hostname)
        .def("str", &IpAddr::str)
        .def("set_subnet_mask", static_cast<Result<void> (IpAddr::*)(const std::string &)>(&IpAddr::set_subnet_mask))
        .def("has_subnet", &IpAddr::has_subnet)
        .def("subnet_value", &IpAddr::subnet_value)
        .def("dump_subnet", &IpAddr::dump_subnet)
        .def("is_same_subnet", static_cast<bool (IpAddr::*)(const IpAddr &) const>(&IpAddr::is_same_subnet))
        .def("__eq__", &IpAddr::operator==)
        .def("__str__", &IpAddr::str);

    pybind11::class_<NetInterface> net_interface(m_net, "NetInterface");
    pybind11::enum_<NetInterface::Kind>(net_interface, "Kind")
        .value("unknown", NetInterface::Kind::unknown)
        .value("loopback", NetInterface::Kind::loopback)
        .value("ethernet", NetInterface::Kind::ethernet)
        .value("wifi", NetInterface::Kind::wifi)
        .value("cellular", NetInterface::Kind::cellular)
        .value("tunnel", NetInterface::Kind::tunnel)
        .value("ppp", NetInterface::Kind::ppp)
        .value("bridge", NetInterface::Kind::bridge);
    pybind11::class_<NetInterface::Address>(net_interface, "Address")
        .def_readonly("addr", &NetInterface::Address::addr)
        .def_readonly("netmask", &NetInterface::Address::netmask)
        .def_readonly("extra_addr", &NetInterface::Address::extra_addr);
    net_interface
        .def_static("get_all_interfaces",
                    &NetInterface::get_all_interfaces,
                    pybind11::call_guard<pybind11::gil_scoped_release>())
        .def_static("kind_str", &NetInterface::kind_str)
        .def("name", &NetInterface::name)
        .def("mac_addr", &NetInterface::mac_addr)
        .def("index", &NetInterface::index)
        .def("kind", &NetInterface::kind)
        .def("hardware", &NetInterface::hardware)
        .def("mtu", &NetInterface::mtu)
        .def("link_speed_bps", &NetInterface::link_speed_bps)
        .def("addresses", &NetInterface::addresses)
        .def("addr4", &NetInterface::addr4)
        .def("netmask4", &NetInterface::netmask4)
        .def("extra_addr4", &NetInterface::extra_addr4)
        .def("addr6", &NetInterface::addr6)
        .def("netmask6", &NetInterface::netmask6)
        .def("extra_addr6", &NetInterface::extra_addr6)
        .def("up", &NetInterface::up)
        .def("broadcast", &NetInterface::broadcast)
        .def("loopback", &NetInterface::loopback)
        .def("point2point", &NetInterface::point2point)
        .def("running", &NetInterface::running)
        .def("noarp", &NetInterface::noarp)
        .def("promisc", &NetInterface::promisc)
        .def("notrailers", &NetInterface::notrailers)
        .def("master", &NetInterface::master)
        .def("slave", &NetInterface::slave)
        .def("all_multicast", &NetInterface::all_multicast)
        .def("supports_multicast", &NetInterface::supports_multicast);

    pybind11::module m_route = m_net.def_submodule("route", "sihd::net::route");
    pybind11::class_<route::Route>(m_route, "Route")
        .def_readonly("interface_name", &route::Route::interface_name)
        .def_readonly("gateway", &route::Route::gateway)
        .def_readonly("source", &route::Route::source)
        .def_readonly("metric", &route::Route::metric);
    m_route.def("find", &route::find, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("defaults_ipv4", &route::defaults_ipv4, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("defaults_ipv6", &route::defaults_ipv6, pybind11::call_guard<pybind11::gil_scoped_release>());

    pybind11::module m_dns = m_net.def_submodule("dns", "sihd::net::dns");
    pybind11::class_<dns::DnsServer>(m_dns, "DnsServer")
        .def_readonly("addr", &dns::DnsServer::addr)
        .def_readonly("interface_name", &dns::DnsServer::interface_name);
    m_dns.def("servers",
              &dns::servers,
              pybind11::arg("resolver_timeout") = dns::default_resolver_timeout,
              pybind11::call_guard<pybind11::gil_scoped_release>());

    pybind11::module m_wifi = m_net.def_submodule("wifi", "sihd::net::wifi");
    pybind11::class_<wifi::Link>(m_wifi, "Link")
        .def_readonly("ssid", &wifi::Link::ssid)
        .def_readonly("bssid", &wifi::Link::bssid)
        .def_readonly("signal_dbm", &wifi::Link::signal_dbm)
        .def_readonly("frequency_mhz", &wifi::Link::frequency_mhz)
        .def_readonly("tx_bitrate_bps", &wifi::Link::tx_bitrate_bps);
    m_wifi.def("link", &wifi::link, pybind11::call_guard<pybind11::gil_scoped_release>());

    pybind11::enum_<NetMonitorEventType>(m_net, "NetMonitorEventType")
        .value("unknown", NetMonitorEventType::unknown)
        .value("link", NetMonitorEventType::link)
        .value("address", NetMonitorEventType::address)
        .value("route", NetMonitorEventType::route)
        .value("overflow", NetMonitorEventType::overflow);
    pybind11::class_<NetMonitorEvent>(m_net, "NetMonitorEvent")
        .def(pybind11::init<>())
        .def_readonly("type", &NetMonitorEvent::type)
        .def_readonly("removed", &NetMonitorEvent::removed)
        .def_readonly("interface_name", &NetMonitorEvent::interface_name)
        .def_readonly("address", &NetMonitorEvent::address)
        .def("type_str", &NetMonitorEvent::type_str);
    pybind11::class_<NetMonitor>(m_net, "NetMonitor")
        .def(pybind11::init(&make_net_monitor), pybind11::arg("run_timeout") = pybind11::none())
        .def("set_run_timeout", &NetMonitor::set_run_timeout)
        .def("run", &NetMonitor::run, pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("events", &NetMonitor::events);

    pybind11::class_<DeviceTcpClient, Device, SmartNodePtr<DeviceTcpClient>>(m_net, "DeviceTcpClient")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_host", &DeviceTcpClient::set_host)
        .def("set_port", &DeviceTcpClient::set_port)
        .def("set_unix_path", &DeviceTcpClient::set_unix_path)
        .def("set_buffer_capacity", &DeviceTcpClient::set_buffer_capacity)
        .def("set_poll_timeout", &DeviceTcpClient::set_poll_timeout)
        .def("set_connect_timeout", &DeviceTcpClient::set_connect_timeout)
        .def("set_reconnect_interval", &DeviceTcpClient::set_reconnect_interval);

    pybind11::class_<DeviceTcpServer, Device, SmartNodePtr<DeviceTcpServer>>(m_net, "DeviceTcpServer")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_host", &DeviceTcpServer::set_host)
        .def("set_port", &DeviceTcpServer::set_port)
        .def("set_unix_path", &DeviceTcpServer::set_unix_path)
        .def("set_buffer_capacity", &DeviceTcpServer::set_buffer_capacity)
        .def("set_poll_timeout", &DeviceTcpServer::set_poll_timeout)
        .def("set_poll_limit", &DeviceTcpServer::set_poll_limit)
        .def("set_queue_size", &DeviceTcpServer::set_queue_size)
        .def("set_max_clients", &DeviceTcpServer::set_max_clients);

    pybind11::class_<DeviceUdpSender, Device, SmartNodePtr<DeviceUdpSender>>(m_net, "DeviceUdpSender")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_host", &DeviceUdpSender::set_host)
        .def("set_port", &DeviceUdpSender::set_port)
        .def("set_unix_path", &DeviceUdpSender::set_unix_path);

    pybind11::class_<DeviceUdpReceiver, Device, SmartNodePtr<DeviceUdpReceiver>>(m_net, "DeviceUdpReceiver")
        .def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>())
        .def("set_host", &DeviceUdpReceiver::set_host)
        .def("set_port", &DeviceUdpReceiver::set_port)
        .def("set_buffer_capacity", &DeviceUdpReceiver::set_buffer_capacity)
        .def("set_poll_timeout", &DeviceUdpReceiver::set_poll_timeout);
}

namespace
{

void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_net", &PyNetApi::add_net_api);
}

} // namespace

} // namespace sihd::py
