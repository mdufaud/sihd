#include <memory>
#include <string>
#include <vector>

// clang-format off
#include <sihd/lua/wifi/LuaWifiApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/checked.hpp>
// clang-format on

#include <sihd/lua/LuaGil.hpp>
#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/link.hpp>
#include <sihd/wifi/scan.hpp>

namespace sihd::lua
{

using namespace sihd::util;

namespace
{

// luabridge copies the values it returns: the copies must share one monitor
class LuaWifiMonitor
{
    public:
        static Result<LuaWifiMonitor> open(const std::string & interface_name, luabridge::LuaRef run_timeout)
        {
            LuaWifiMonitor monitor;
            monitor._monitor = std::make_shared<wifi::Monitor>();
            if (!run_timeout.isNil())
                monitor._monitor->set_run_timeout(to_duration(run_timeout));
            auto opened = monitor._monitor->open(interface_name);
            SIHD_UNEXPECTED_RETURN(opened);
            return monitor;
        }

        void set_run_timeout(luabridge::LuaRef timeout) { _monitor->set_run_timeout(to_duration(timeout)); }
        bool run(lua_State *state)
        {
            LuaGilRelease release(state);
            return _monitor->run();
        }
        std::vector<wifi::Monitor::Event> events() const { return _monitor->events(); }

    private:
        std::shared_ptr<wifi::Monitor> _monitor;
};

Result<void> connect(lua_State *state,
                     const std::string & interface_name,
                     const std::string & ssid,
                     luabridge::LuaRef passphrase_or_timeout,
                     luabridge::LuaRef timeout_after_passphrase)
{
    if (timeout_after_passphrase.isNil())
    {
        const Duration timeout = to_duration(passphrase_or_timeout);
        LuaGilRelease release(state);
        return wifi::connect(interface_name, ssid, timeout);
    }
    if (!passphrase_or_timeout.isString())
        return std::unexpected(Error(ErrorCode::invalid_argument, "connect passphrase must be a string"));
    const std::string passphrase = passphrase_or_timeout.tostring();
    const Duration timeout = to_duration(timeout_after_passphrase);
    LuaGilRelease release(state);
    return wifi::connect(interface_name, ssid, passphrase, timeout);
}

} // namespace

void LuaWifiApi::load_all(Vm & vm)
{
    LuaWifiApi::load_base(vm);
}

void LuaWifiApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("wifi")
        .beginNamespace("Security")
        .addVariable("unknown", wifi::Security::unknown)
        .addVariable("open", wifi::Security::open)
        .addVariable("owe", wifi::Security::owe)
        .addVariable("wep", wifi::Security::wep)
        .addVariable("personal", wifi::Security::personal)
        .addVariable("enterprise", wifi::Security::enterprise)
        .endNamespace()
        .beginNamespace("Backend")
        .addVariable("network_manager", wifi::Backend::network_manager)
        .addVariable("iwd", wifi::Backend::iwd)
        .addVariable("wpa_supplicant", wifi::Backend::wpa_supplicant)
        .addVariable("wlan_service", wifi::Backend::wlan_service)
        .endNamespace()
        .beginNamespace("State")
        .addVariable("disconnected", wifi::State::disconnected)
        .addVariable("connecting", wifi::State::connecting)
        .addVariable("connected", wifi::State::connected)
        .addVariable("disconnecting", wifi::State::disconnecting)
        .endNamespace()

        .beginClass<wifi::Link>("Link")
        .addConstructor<void (*)()>()
        .addProperty("ssid", &wifi::Link::ssid, false)
        .addProperty("bssid", &wifi::Link::bssid, false)
        .addProperty("signal_dbm", &wifi::Link::signal_dbm, false)
        .addProperty("frequency_mhz", &wifi::Link::frequency_mhz, false)
        .addProperty("tx_bitrate_bps", &wifi::Link::tx_bitrate_bps, false)
        .endClass()
        .beginClass<wifi::AccessPoint>("AccessPoint")
        .addConstructor<void (*)()>()
        .addProperty("ssid", &wifi::AccessPoint::ssid, false)
        .addProperty("bssid", &wifi::AccessPoint::bssid, false)
        .addProperty("signal_dbm", &wifi::AccessPoint::signal_dbm, false)
        .addProperty("frequency_mhz", &wifi::AccessPoint::frequency_mhz, false)
        .addProperty("security", &wifi::AccessPoint::security, false)
        .addProperty("seen_ago", &wifi::AccessPoint::seen_ago, false)
        .addProperty("associated", &wifi::AccessPoint::associated, false)
        .endClass()
        .beginClass<wifi::KnownNetwork>("KnownNetwork")
        .addConstructor<void (*)()>()
        .addProperty("ssid", &wifi::KnownNetwork::ssid, false)
        .addProperty("security", &wifi::KnownNetwork::security, false)
        .endClass()
        .beginClass<LuaWifiMonitor>("Monitor")
        .addStaticFunction("open", checked<&LuaWifiMonitor::open>)
        .addFunction("set_run_timeout", &LuaWifiMonitor::set_run_timeout)
        .addFunction("run", &LuaWifiMonitor::run)
        .addFunction("events", &LuaWifiMonitor::events)
        .endClass()
        // luabridge nests no class in a class: the Monitor table hosts Event like a namespace
        .beginNamespace("Monitor")
        .beginClass<wifi::Monitor::Event>("Event")
        .addConstructor<void (*)()>()
        .addStaticProperty("Type",
                           [](lua_State *state) {
                               luabridge::LuaRef types = luabridge::newTable(state);
                               types["scan_done"] = wifi::Monitor::Event::Type::scan_done;
                               types["state_changed"] = wifi::Monitor::Event::Type::state_changed;
                               return types;
                           })
        .addProperty("type", &wifi::Monitor::Event::type, false)
        .addProperty("state", &wifi::Monitor::Event::state, false)
        .endClass()
        .endNamespace()

        .addFunction("link", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::link(interface_name);
                     }>)
        .addFunction("access_points", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::access_points(interface_name);
                     }>)
        .addFunction(
            "security_of",
            +[](const std::string & information_elements, bool privacy) {
                return wifi::security_of(ArrUByteView(information_elements), privacy);
            })
        .addFunction("scan",
                     checked<+[](lua_State *state, const std::string & interface_name, luabridge::LuaRef timeout) {
                         const Duration scan_timeout = to_duration(timeout);
                         LuaGilRelease release(state);
                         return wifi::scan(interface_name, scan_timeout);
                     }>)

        .addFunction("backend", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::backend(interface_name);
                     }>)
        .addFunction("state", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::state(interface_name);
                     }>)
        .addFunction("connect", checked<&connect>)

        .addFunction("disconnect", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::disconnect(interface_name);
                     }>)
        .addFunction("known_networks", checked<+[](lua_State *state, const std::string & interface_name) {
                         LuaGilRelease release(state);
                         return wifi::known_networks(interface_name);
                     }>)
        .addFunction("forget",
                     checked<+[](lua_State *state, const std::string & interface_name, const std::string & ssid) {
                         LuaGilRelease release(state);
                         return wifi::forget(interface_name, ssid);
                     }>)
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
