#ifndef __SIHD_LUA_LUANETAPI_HPP__
#define __SIHD_LUA_LUANETAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

#include <sihd/net/NetInterface.hpp>
#include <sihd/net/NetMonitor.hpp>

template <>
struct luabridge::Stack<sihd::net::NetInterface::Kind>: luabridge::Enum<sihd::net::NetInterface::Kind,
                                                                        sihd::net::NetInterface::Kind::unknown,
                                                                        sihd::net::NetInterface::Kind::loopback,
                                                                        sihd::net::NetInterface::Kind::ethernet,
                                                                        sihd::net::NetInterface::Kind::wifi,
                                                                        sihd::net::NetInterface::Kind::cellular,
                                                                        sihd::net::NetInterface::Kind::tunnel,
                                                                        sihd::net::NetInterface::Kind::ppp,
                                                                        sihd::net::NetInterface::Kind::bridge>
{
};

template <>
struct luabridge::Stack<sihd::net::NetMonitorEventType>: luabridge::Enum<sihd::net::NetMonitorEventType,
                                                                         sihd::net::NetMonitorEventType::unknown,
                                                                         sihd::net::NetMonitorEventType::link,
                                                                         sihd::net::NetMonitorEventType::address,
                                                                         sihd::net::NetMonitorEventType::route,
                                                                         sihd::net::NetMonitorEventType::overflow>
{
};

namespace sihd::lua
{

class LuaNetApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaNetApi() = delete;
        ~LuaNetApi() = delete;
};

} // namespace sihd::lua

#endif
