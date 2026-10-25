#ifndef __SIHD_LUA_LUAWIFIAPI_HPP__
#define __SIHD_LUA_LUAWIFIAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/scan.hpp>

template <>
struct luabridge::Stack<sihd::wifi::Security>: luabridge::Enum<sihd::wifi::Security,
                                                               sihd::wifi::Security::unknown,
                                                               sihd::wifi::Security::open,
                                                               sihd::wifi::Security::owe,
                                                               sihd::wifi::Security::wep,
                                                               sihd::wifi::Security::personal,
                                                               sihd::wifi::Security::enterprise>
{
};

template <>
struct luabridge::Stack<sihd::wifi::Backend>: luabridge::Enum<sihd::wifi::Backend,
                                                              sihd::wifi::Backend::network_manager,
                                                              sihd::wifi::Backend::iwd,
                                                              sihd::wifi::Backend::wpa_supplicant,
                                                              sihd::wifi::Backend::wlan_service>
{
};

template <>
struct luabridge::Stack<sihd::wifi::State>: luabridge::Enum<sihd::wifi::State,
                                                            sihd::wifi::State::disconnected,
                                                            sihd::wifi::State::connecting,
                                                            sihd::wifi::State::connected,
                                                            sihd::wifi::State::disconnecting>
{
};

template <>
struct luabridge::Stack<sihd::wifi::Monitor::Event::Type>
    : luabridge::Enum<sihd::wifi::Monitor::Event::Type,
                      sihd::wifi::Monitor::Event::Type::scan_done,
                      sihd::wifi::Monitor::Event::Type::state_changed>
{
};

namespace sihd::lua
{

class LuaWifiApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaWifiApi() = delete;
        ~LuaWifiApi() = delete;
};

} // namespace sihd::lua

#endif
