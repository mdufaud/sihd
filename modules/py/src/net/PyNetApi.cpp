#include <sihd/py/net/PyNetApi.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include <sihd/core/Device.hpp>

#include <sihd/net/DeviceTcpClient.hpp>
#include <sihd/net/DeviceTcpServer.hpp>
#include <sihd/net/DeviceUdpReceiver.hpp>
#include <sihd/net/DeviceUdpSender.hpp>
#include <sihd/net/IpAddr.hpp>

namespace sihd::py
{

using namespace sihd::net;
using sihd::core::Device;
using sihd::util::Node;
using sihd::util::SmartNodePtr;

SIHD_LOGGER;

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
        .def("set_subnet_mask", static_cast<bool (IpAddr::*)(std::string_view)>(&IpAddr::set_subnet_mask))
        .def("has_subnet", &IpAddr::has_subnet)
        .def("subnet_value", &IpAddr::subnet_value)
        .def("dump_subnet", &IpAddr::dump_subnet)
        .def("is_same_subnet", static_cast<bool (IpAddr::*)(const IpAddr &) const>(&IpAddr::is_same_subnet))
        .def("__eq__", &IpAddr::operator==)
        .def("__str__", &IpAddr::str);

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

static void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_net", &PyNetApi::add_net_api);
}

} // namespace sihd::py
