#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// clang-format off
#include <sihd/lua/bt/LuaBtApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <sihd/lua/checked.hpp>
#include <luabridge3/LuaBridge/Map.h>
#include <luabridge3/LuaBridge/Vector.h>
// clang-format on

#include <sihd/bt/Advertiser.hpp>
#include <sihd/bt/Gatt.hpp>
#include <sihd/bt/GattServer.hpp>
#include <sihd/bt/Socket.hpp>
#include <sihd/lua/LuaGil.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace sihd::lua
{

using namespace sihd::util;

namespace
{

std::string string_of(const std::vector<uint8_t> & data)
{
    return std::string(data.begin(), data.end());
}

std::vector<uint8_t> vector_of(const std::string & data)
{
    return std::vector<uint8_t>(data.begin(), data.end());
}

template <typename Key>
std::map<Key, std::string> string_map(const std::map<Key, std::vector<uint8_t>> & data)
{
    std::map<Key, std::string> result;
    for (const auto & [key, value] : data)
        result.emplace(key, string_of(value));
    return result;
}

template <typename Key>
std::map<Key, std::vector<uint8_t>> vector_map(const std::map<Key, std::string> & data)
{
    std::map<Key, std::vector<uint8_t>> result;
    for (const auto & [key, value] : data)
        result.emplace(key, vector_of(value));
    return result;
}

template <typename T>
Result<bool> wait_ready(T *self, luabridge::LuaRef timeout, lua_State *state)
{
    const Result<Duration> duration = duration_of(timeout);
    SIHD_UNEXPECTED_RETURN(duration);
    LuaGilRelease release(state);
    return self->wait_ready(*duration);
}

template <typename T, typename ClassBinding>
ClassBinding bind_service(ClassBinding binding)
{
    binding
        .addFunction(
            "start",
            +[](T *self, lua_State *state) -> bool {
                LuaGilRelease release(state);
                return self->start();
            })
        .addFunction(
            "stop",
            +[](T *self, lua_State *state) -> bool {
                LuaGilRelease release(state);
                return self->stop();
            })
        .addFunction(
            "is_running",
            +[](T *self) -> bool { return self->is_running(); })
        .addFunction(
            "is_ready",
            +[](T *self) -> bool { return self->is_ready(); })
        .addFunction("wait_ready", checked<&wait_ready<T>>)
        .addFunction("set_service_wait_stop", +[](T *self, bool active) { self->set_service_wait_stop(active); });
    return binding;
}

luabridge::Namespace bt_namespace(lua_State *state)
{
    return luabridge::getGlobalNamespace(state).beginNamespace("sihd").beginNamespace("bt");
}

void bind_adapter(lua_State *state)
{
    bt_namespace(state)
        .beginNamespace("AddressType")
        .addVariable("public_address", bt::AddressType::public_address)
        .addVariable("random", bt::AddressType::random)
        .addVariable("unspecified", bt::AddressType::unspecified)
        .endNamespace()
        .beginClass<bt::AdapterInfo>("AdapterInfo")
        .addConstructor<void (*)()>()
        .addProperty("identifier", &bt::AdapterInfo::identifier)
        .addProperty("address", &bt::AdapterInfo::address)
        .addProperty("name", &bt::AdapterInfo::name)
        .addProperty("alias", &bt::AdapterInfo::alias)
        .addProperty("powered", &bt::AdapterInfo::powered)
        .addProperty("discoverable", &bt::AdapterInfo::discoverable)
        .addProperty("pairable", &bt::AdapterInfo::pairable)
        .addProperty("discovering", &bt::AdapterInfo::discovering)
        .endClass()

        .addFunction("bluetooth_enabled", checked<+[](lua_State *state) {
                         LuaGilRelease release(state);
                         return bt::bluetooth_enabled();
                     }>)
        .addFunction("list_adapters", checked<+[](lua_State *state) {
                         LuaGilRelease release(state);
                         return bt::list_adapters();
                     }>)

        .addFunction("set_adapter_powered", checked<+[](lua_State *state, size_t index, bool powered) {
                         LuaGilRelease release(state);
                         return bt::set_adapter_powered(index, powered);
                     }>)
        .addFunction("set_adapter_discoverable", checked<+[](lua_State *state, size_t index, bool discoverable) {
                         LuaGilRelease release(state);
                         return bt::set_adapter_discoverable(index, discoverable);
                     }>)
        .addFunction("set_adapter_pairable", checked<+[](lua_State *state, size_t index, bool pairable) {
                         LuaGilRelease release(state);
                         return bt::set_adapter_pairable(index, pairable);
                     }>)
        .addFunction("set_adapter_alias", checked<+[](lua_State *state, size_t index, const std::string & alias) {
                         LuaGilRelease release(state);
                         return bt::set_adapter_alias(index, alias);
                     }>);
}

void bind_scanner(lua_State *state)
{
    bt_namespace(state)
        .beginNamespace("Transport")
        .addVariable("any", bt::Transport::any)
        .addVariable("le", bt::Transport::le)
        .addVariable("classic", bt::Transport::classic)
        .endNamespace()
        .beginClass<bt::ScanFilter>("ScanFilter")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("transport", &bt::ScanFilter::transport)
        .addPropertyReadWrite("uuids", &bt::ScanFilter::uuids)
        .addPropertyReadWrite("min_rssi", &bt::ScanFilter::min_rssi)
        .endClass()
        .beginClass<bt::ScanResult>("ScanResult")
        .addConstructor<void (*)()>()
        .addProperty("identifier", &bt::ScanResult::identifier)
        .addProperty("address", &bt::ScanResult::address)
        .addProperty("address_type", &bt::ScanResult::address_type)
        .addProperty("rssi", &bt::ScanResult::rssi)
        .addProperty("tx_power", &bt::ScanResult::tx_power)
        .addProperty("connectable", &bt::ScanResult::connectable)
        .addProperty(
            "manufacturer_data",
            +[](const bt::ScanResult *self) { return string_map(self->manufacturer_data); })
        .addProperty(
            "service_data",
            +[](const bt::ScanResult *self) { return string_map(self->service_data); })
        .addProperty("uuids", &bt::ScanResult::uuids)
        .addProperty("appearance", &bt::ScanResult::appearance)
        .addProperty("class_of_device", &bt::ScanResult::class_of_device)
        .addProperty("timestamp", &bt::ScanResult::timestamp)
        .endClass();

    bind_service<bt::Scanner>(bt_namespace(state).deriveClass<bt::Scanner, Named>("Scanner"))
        .addConstructorFrom<SmartNodePtr<bt::Scanner>, void(const std::string &, Node *)>()
        .addFunction("set_conf", checked<&LuaUtilApi::configurable_set_conf<bt::Scanner>>)
        .addFunction("set_adapter", &bt::Scanner::set_adapter)
        .addFunction("set_filter", &bt::Scanner::set_filter)
        .addFunction("set_lost_timeout", checked<+[](bt::Scanner *self, luabridge::LuaRef timeout) -> Result<bool> {
                         const Result<Duration> duration = duration_of(timeout);
                         SIHD_UNEXPECTED_RETURN(duration);
                         return self->set_lost_timeout(*duration);
                     }>)
        .addFunction("scan_for",
                     checked<+[](bt::Scanner *self, luabridge::LuaRef duration, lua_State *state) -> Result<bool> {
                         const Result<Duration> timeout = duration_of(duration);
                         SIHD_UNEXPECTED_RETURN(timeout);
                         LuaGilRelease release(state);
                         return self->scan_for(*timeout);
                     }>)
        .addFunction(
            "results",
            +[](bt::Scanner *self) { return self->results(); })
        .addFunction("last_result", &bt::Scanner::last_result)
        .addFunction("last_event", &bt::Scanner::last_event)
        .addFunction("add_observer", checked<&LuaUtilApi::LuaObservers<bt::Scanner>::add>)
        .addFunction("remove_observers", &LuaUtilApi::LuaObservers<bt::Scanner>::remove_all)
        .endClass()
        // luabridge nests no class in a class: the Scanner table hosts Event like a namespace
        .beginNamespace("Scanner")
        .beginNamespace("Event")
        .addVariable("found", bt::Scanner::Event::found)
        .addVariable("updated", bt::Scanner::Event::updated)
        .addVariable("lost", bt::Scanner::Event::lost)
        .endNamespace()
        .endNamespace();
}

void bind_gatt(lua_State *state)
{
    bt_namespace(state)
        .beginNamespace("WriteType")
        .addVariable("with_response", bt::WriteType::with_response)
        .addVariable("without_response", bt::WriteType::without_response)
        .endNamespace()
        .beginClass<bt::GattDescriptor>("GattDescriptor")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("uuid", &bt::GattDescriptor::uuid)
        .addPropertyReadWrite("handle", &bt::GattDescriptor::handle)
        .endClass()
        .beginClass<bt::GattCharacteristic>("GattCharacteristic")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("uuid", &bt::GattCharacteristic::uuid)
        .addPropertyReadWrite("handle", &bt::GattCharacteristic::handle)
        .addPropertyReadWrite("properties", &bt::GattCharacteristic::properties)
        .addPropertyReadWrite("descriptors", &bt::GattCharacteristic::descriptors)
        .endClass()
        // luabridge nests no class in a class: the GattCharacteristic table hosts Property like a namespace
        .beginNamespace("GattCharacteristic")
        .beginNamespace("Property")
        .addVariable("broadcast", static_cast<int>(bt::GattCharacteristic::Property::broadcast))
        .addVariable("read", static_cast<int>(bt::GattCharacteristic::Property::read))
        .addVariable("write_without_response",
                     static_cast<int>(bt::GattCharacteristic::Property::write_without_response))
        .addVariable("write", static_cast<int>(bt::GattCharacteristic::Property::write))
        .addVariable("notify", static_cast<int>(bt::GattCharacteristic::Property::notify))
        .addVariable("indicate", static_cast<int>(bt::GattCharacteristic::Property::indicate))
        .addVariable("signed_write", static_cast<int>(bt::GattCharacteristic::Property::signed_write))
        .addVariable("extended_properties", static_cast<int>(bt::GattCharacteristic::Property::extended_properties))
        .endNamespace()
        .endNamespace()
        .beginClass<bt::GattService>("GattService")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("uuid", &bt::GattService::uuid)
        .addPropertyReadWrite("handle", &bt::GattService::handle)
        .addPropertyReadWrite("primary", &bt::GattService::primary)
        .addPropertyReadWrite("characteristics", &bt::GattService::characteristics)
        .endClass()
        .beginClass<bt::GattNotification>("GattNotification")
        .addConstructor<void (*)()>()
        .addProperty("handle", &bt::GattNotification::handle)
        .addProperty("uuid", &bt::GattNotification::uuid)
        .addProperty(
            "value",
            +[](const bt::GattNotification *self) { return string_of(self->value); })
        .addProperty("timestamp", &bt::GattNotification::timestamp)
        .endClass();
}

Result<void> set_pairing_handler(bt::Device *self, luabridge::LuaRef handler)
{
    if (handler.isNil())
    {
        self->set_pairing_handler(nullptr);
        return {};
    }
    if (!handler.isFunction())
        return std::unexpected(Error(ErrorCode::invalid_argument, "set_pairing_handler: expected a function or nil"));
    auto callback = std::make_shared<LuaUtilApi::LuaCallback>(handler.state(), handler);
    // the system agent asks from a backend thread, an answer that fails is a refusal
    self->set_pairing_handler([callback](const bt::PairingRequest & request) {
        return callback->call<bt::PairingAnswer>(request).value_or(bt::PairingAnswer {});
    });
    return {};
}

void bind_device(lua_State *state)
{
    bt_namespace(state)
        .beginClass<bt::DeviceInfo>("DeviceInfo")
        .addConstructor<void (*)()>()
        .addProperty("address", &bt::DeviceInfo::address)
        .addProperty("address_type", &bt::DeviceInfo::address_type)
        .addProperty("name", &bt::DeviceInfo::name)
        .addProperty("paired", &bt::DeviceInfo::paired)
        .addProperty("connected", &bt::DeviceInfo::connected)
        .addProperty("trusted", &bt::DeviceInfo::trusted)
        .addProperty("appearance", &bt::DeviceInfo::appearance)
        .addProperty("class_of_device", &bt::DeviceInfo::class_of_device)
        .addProperty("uuids", &bt::DeviceInfo::uuids)
        .endClass()
        .beginClass<bt::PairingRequest>("PairingRequest")
        .addConstructor<void (*)()>()
        .addProperty("address", &bt::PairingRequest::address)
        .addProperty("kind", &bt::PairingRequest::kind)
        .addProperty("passkey", &bt::PairingRequest::passkey)
        .addProperty("pin", &bt::PairingRequest::pin)
        .endClass()
        .beginNamespace("PairingRequest")
        .beginNamespace("Kind")
        .addVariable("confirm", bt::PairingRequest::Kind::confirm)
        .addVariable("confirm_passkey", bt::PairingRequest::Kind::confirm_passkey)
        .addVariable("display_passkey", bt::PairingRequest::Kind::display_passkey)
        .addVariable("enter_passkey", bt::PairingRequest::Kind::enter_passkey)
        .addVariable("display_pin", bt::PairingRequest::Kind::display_pin)
        .addVariable("enter_pin", bt::PairingRequest::Kind::enter_pin)
        .endNamespace()
        .endNamespace()
        .beginClass<bt::PairingAnswer>("PairingAnswer")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("accept", &bt::PairingAnswer::accept)
        .addPropertyReadWrite("passkey", &bt::PairingAnswer::passkey)
        .addPropertyReadWrite("pin", &bt::PairingAnswer::pin)
        .endClass();

    bind_service<bt::Device>(bt_namespace(state).deriveClass<bt::Device, Named>("Device"))
        .addConstructorFrom<SmartNodePtr<bt::Device>, void(const std::string &, Node *)>()
        .addFunction("set_conf", checked<&LuaUtilApi::configurable_set_conf<bt::Device>>)
        .addFunction("set_address", &bt::Device::set_address)
        .addFunction("set_adapter", &bt::Device::set_adapter)
        .addFunction("set_pairing_handler", checked<&set_pairing_handler>)

        .addFunction("info", checked<+[](bt::Device *self, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->info();
                     }>)
        .addFunction("pair", checked<+[](bt::Device *self, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->pair();
                     }>)
        .addFunction("cancel_pairing", checked<+[](bt::Device *self, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->cancel_pairing();
                     }>)
        .addFunction("unpair", checked<+[](bt::Device *self, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->unpair();
                     }>)

        .addFunction("set_trusted", checked<+[](bt::Device *self, bool trusted, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->set_trusted(trusted);
                     }>)

        .addFunction("services", checked<+[](bt::Device *self, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->services();
                     }>)
        .addFunction("read_characteristic",
                     checked<+[](bt::Device *self, const bt::GattCharacteristic & characteristic, lua_State *state)
                                 -> Result<std::string> {
                         LuaGilRelease release(state);
                         auto value = self->read_characteristic(characteristic);
                         SIHD_UNEXPECTED_RETURN(value);
                         return string_of(*value);
                     }>)
        .addFunction("write_characteristic",
                     checked<+[](bt::Device *self,
                                 const bt::GattCharacteristic & characteristic,
                                 const std::string & value,
                                 bt::WriteType type,
                                 lua_State *state) {
                         LuaGilRelease release(state);
                         return self->write_characteristic(characteristic, ArrUByteView(value), type);
                     }>)

        .addFunction("read_descriptor",
                     checked<+[](bt::Device *self, const bt::GattDescriptor & descriptor, lua_State *state)
                                 -> Result<std::string> {
                         LuaGilRelease release(state);
                         auto value = self->read_descriptor(descriptor);
                         SIHD_UNEXPECTED_RETURN(value);
                         return string_of(*value);
                     }>)
        .addFunction("write_descriptor",
                     checked<+[](bt::Device *self,
                                 const bt::GattDescriptor & descriptor,
                                 const std::string & value,
                                 lua_State *state) {
                         LuaGilRelease release(state);
                         return self->write_descriptor(descriptor, ArrUByteView(value));
                     }>)

        .addFunction("subscribe",
                     checked<+[](bt::Device *self, const bt::GattCharacteristic & characteristic, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->subscribe(characteristic);
                     }>)
        .addFunction("unsubscribe",
                     checked<+[](bt::Device *self, const bt::GattCharacteristic & characteristic, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->unsubscribe(characteristic);
                     }>)
        .addFunction("mtu", checked<+[](bt::Device *self, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->mtu();
                     }>)

        .addFunction("last_event", &bt::Device::last_event)
        .addFunction("last_notification", &bt::Device::last_notification)
        .addFunction("add_observer", checked<&LuaUtilApi::LuaObservers<bt::Device>::add>)
        .addFunction("remove_observers", &LuaUtilApi::LuaObservers<bt::Device>::remove_all)
        .endClass()
        // luabridge nests no class in a class: the Device table hosts Event like a namespace
        .beginNamespace("Device")
        .beginNamespace("Event")
        .addVariable("connected", bt::Device::Event::connected)
        .addVariable("disconnected", bt::Device::Event::disconnected)
        .addVariable("notification", bt::Device::Event::notification)
        .endNamespace()
        .endNamespace();
}

void bind_local_service(lua_State *state)
{
    bt_namespace(state)
        .beginClass<bt::LocalCharacteristic>("LocalCharacteristic")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("uuid", &bt::LocalCharacteristic::uuid)
        .addPropertyReadWrite("properties", &bt::LocalCharacteristic::properties)
        .addFunction("set_on_read",
                     checked<+[](bt::LocalCharacteristic *self, luabridge::LuaRef function) -> Result<void> {
                         if (function.isNil())
                         {
                             self->on_read = nullptr;
                             return {};
                         }
                         if (!function.isFunction())
                             return std::unexpected(
                                 Error(ErrorCode::invalid_argument, "set_on_read: expected a function or nil"));
                         auto callback = std::make_shared<LuaUtilApi::LuaCallback>(function.state(), function);
                         // the backend asks from its own thread, a failing function reads as an empty value
                         self->on_read = [callback] {
                             return vector_of(callback->call<std::string>().value_or(std::string {}));
                         };
                         return {};
                     }>)

        .addFunction("set_on_write",
                     checked<+[](bt::LocalCharacteristic *self, luabridge::LuaRef function) -> Result<void> {
                         if (function.isNil())
                         {
                             self->on_write = nullptr;
                             return {};
                         }
                         if (!function.isFunction())
                             return std::unexpected(
                                 Error(ErrorCode::invalid_argument, "set_on_write: expected a function or nil"));
                         auto callback = std::make_shared<LuaUtilApi::LuaCallback>(function.state(), function);
                         // a failing function refuses the write
                         self->on_write = [callback](const std::vector<uint8_t> & value) {
                             return callback->call<bool>(string_of(value)).value_or(false);
                         };
                         return {};
                     }>)
        .endClass()
        .beginClass<bt::LocalService>("LocalService")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("uuid", &bt::LocalService::uuid)
        .addPropertyReadWrite("primary", &bt::LocalService::primary)
        .addPropertyReadWrite("characteristics", &bt::LocalService::characteristics)
        .endClass();

    bind_service<bt::GattServer>(bt_namespace(state).deriveClass<bt::GattServer, Named>("GattServer"))
        .addConstructorFrom<SmartNodePtr<bt::GattServer>, void(const std::string &, Node *)>()
        .addFunction("set_conf", checked<&LuaUtilApi::configurable_set_conf<bt::GattServer>>)
        .addFunction("set_adapter", &bt::GattServer::set_adapter)
        .addFunction("add_service", &bt::GattServer::add_service)
        .addFunction("notify",
                     checked<+[](bt::GattServer *self,
                                 const std::string & service_uuid,
                                 const std::string & characteristic_uuid,
                                 const std::string & value,
                                 lua_State *state) {
                         LuaGilRelease release(state);
                         return self->notify(service_uuid, characteristic_uuid, ArrUByteView(value));
                     }>)
        .endClass();
}

void bind_advertiser(lua_State *state)
{
    bt_namespace(state)
        .beginClass<bt::Advertisement>("Advertisement")
        .addConstructor<void (*)()>()
        .addPropertyReadWrite("local_name", &bt::Advertisement::local_name)
        .addPropertyReadWrite("service_uuids", &bt::Advertisement::service_uuids)
        .addProperty(
            "manufacturer_data",
            +[](const bt::Advertisement *self) { return string_map(self->manufacturer_data); },
            +[](bt::Advertisement *self, const std::map<uint16_t, std::string> & data) {
                self->manufacturer_data = vector_map(data);
            })
        .addProperty(
            "service_data",
            +[](const bt::Advertisement *self) { return string_map(self->service_data); },
            +[](bt::Advertisement *self, const std::map<std::string, std::string> & data) {
                self->service_data = vector_map(data);
            })
        .addPropertyReadWrite("connectable", &bt::Advertisement::connectable)
        .endClass();

    bind_service<bt::Advertiser>(bt_namespace(state).deriveClass<bt::Advertiser, Named>("Advertiser"))
        .addConstructorFrom<SmartNodePtr<bt::Advertiser>, void(const std::string &, Node *)>()
        .addFunction("set_conf", checked<&LuaUtilApi::configurable_set_conf<bt::Advertiser>>)
        .addFunction("set_adapter", &bt::Advertiser::set_adapter)
        .addFunction("set_advertisement", &bt::Advertiser::set_advertisement)
        .endClass();
}

Result<std::string> receive(bt::Socket *self, size_t size, lua_State *state)
{
    ArrUByte buffer;
    buffer.reserve(size);
    LuaGilRelease release(state);
    auto received = self->receive(buffer);
    SIHD_UNEXPECTED_RETURN(received);
    return std::string(reinterpret_cast<const char *>(buffer.data()), *received);
}

Result<bt::Socket> accept(bt::Socket *self, luabridge::LuaRef timeout, lua_State *state)
{
    const Result<Duration> duration = duration_of(timeout);
    SIHD_UNEXPECTED_RETURN(duration);
    LuaGilRelease release(state);
    return self->accept(*duration);
}

void bind_socket(lua_State *state)
{
    bt_namespace(state)
        .beginClass<bt::Socket>("Socket")
        .addConstructor<void (*)()>()
        .addFunction(
            "connect_rfcomm",
            checked<+[](bt::Socket *self, const std::string & address, const std::string & uuid, lua_State *state) {
                LuaGilRelease release(state);
                return self->connect_rfcomm(address, uuid);
            }>)
        .addFunction("connect_rfcomm_channel",
                     checked<+[](bt::Socket *self, const std::string & address, uint8_t channel, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->connect_rfcomm_channel(address, channel);
                     }>)
        .addFunction("connect_l2cap",
                     checked<+[](bt::Socket *self,
                                 const std::string & address,
                                 bt::AddressType type,
                                 uint16_t psm,
                                 lua_State *state) {
                         LuaGilRelease release(state);
                         return self->connect_l2cap(address, type, psm);
                     }>)

        .addFunction(
            "listen_rfcomm",
            checked<+[](bt::Socket *self, const std::string & uuid, const std::string & name, lua_State *state) {
                LuaGilRelease release(state);
                return self->listen_rfcomm(uuid, name);
            }>)
        .addFunction("listen_l2cap", checked<+[](bt::Socket *self, uint16_t psm, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->listen_l2cap(psm);
                     }>)

        .addFunction("address", &bt::Socket::address)
        .addFunction("is_open", &bt::Socket::is_open)
        .addFunction("is_listening", &bt::Socket::is_listening)
        .addFunction("psm", &bt::Socket::psm)

        .addFunction("send", checked<+[](bt::Socket *self, const std::string & data, lua_State *state) {
                         LuaGilRelease release(state);
                         return self->send(ArrUByteView(data));
                     }>)
        .addFunction("receive", checked<&receive>)
        .addFunction("accept", checked<&accept>)
        .addFunction("close", &bt::Socket::close)
        .endClass();
}

} // namespace

void LuaBtApi::load_all(Vm & vm)
{
    LuaBtApi::load_base(vm);
}

void LuaBtApi::load_base(Vm & vm)
{
    bind_adapter(vm.lua_state());
    bind_scanner(vm.lua_state());
    bind_gatt(vm.lua_state());
    bind_device(vm.lua_state());
    bind_local_service(vm.lua_state());
    bind_advertiser(vm.lua_state());
    bind_socket(vm.lua_state());
}

} // namespace sihd::lua
