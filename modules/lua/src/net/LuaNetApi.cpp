#include <string>

// clang-format off
#include <sihd/lua/net/LuaNetApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
// clang-format on

#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include <sihd/core/Device.hpp>

#include <sihd/net/DeviceTcpClient.hpp>
#include <sihd/net/DeviceTcpServer.hpp>
#include <sihd/net/DeviceUdpReceiver.hpp>
#include <sihd/net/DeviceUdpSender.hpp>
#include <sihd/net/IpAddr.hpp>

namespace sihd::lua
{

using namespace sihd::net;
using sihd::core::Device;
using sihd::util::Node;
using sihd::util::SmartNodePtr;

void LuaNetApi::load_all(Vm & vm)
{
    LuaNetApi::load_base(vm);
}

void LuaNetApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("net")
        // address value type
        .beginClass<IpAddr>("IpAddr")
        .addConstructor<void (*)()>()
        .addFunction("set_hostname",
                     +[](IpAddr *self, const std::string & hostname) { self->set_hostname(hostname); })
        .addFunction("empty", &IpAddr::empty)
        .addFunction("has_ip", &IpAddr::has_ip)
        .addFunction("is_ipv4", &IpAddr::is_ipv4)
        .addFunction("is_ipv6", &IpAddr::is_ipv6)
        .addFunction("port", &IpAddr::port)
        .addFunction("fetch_hostname", &IpAddr::fetch_hostname)
        .addFunction("hostname", +[](IpAddr *self) -> std::string { return self->hostname(); })
        .addFunction("str", &IpAddr::str)
        .addFunction("set_subnet_mask",
                     +[](IpAddr *self, const std::string & mask) -> bool { return self->set_subnet_mask(mask); })
        .addFunction("has_subnet", &IpAddr::has_subnet)
        .addFunction("subnet_value", &IpAddr::subnet_value)
        .addFunction("dump_subnet", &IpAddr::dump_subnet)
        .addFunction("is_same_subnet",
                     +[](IpAddr *self, const IpAddr & other) -> bool { return self->is_same_subnet(other); })
        .endClass()
        // factory: net.ip_addr("host") or net.ip_addr("host", port)
        .addFunction("ip_addr",
                     +[](const std::string & host, luabridge::LuaRef port) -> IpAddr {
                         if (port.isNil())
                             return IpAddr(host);
                         return IpAddr(host, static_cast<int>(port));
                     })
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
