#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sihd/bt/Advertiser.hpp>
#include <sihd/bt/Device.hpp>
#include <sihd/bt/Gatt.hpp>
#include <sihd/bt/GattServer.hpp>
#include <sihd/bt/Scanner.hpp>
#include <sihd/bt/Socket.hpp>
#include <sihd/bt/adapter.hpp>
#include <sihd/py/bt/PyBtApi.hpp>
#include <sihd/py/util/PyUtilApi.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include "../expected_caster.hpp"

namespace sihd::py
{

using namespace sihd::util;

namespace
{

pybind11::bytes bytes_of(const std::vector<uint8_t> & data)
{
    return pybind11::bytes(reinterpret_cast<const char *>(data.data()), data.size());
}

std::vector<uint8_t> vector_of(const pybind11::bytes & data)
{
    const std::string text = data;
    return std::vector<uint8_t>(text.begin(), text.end());
}

template <typename Key>
std::map<Key, pybind11::bytes> bytes_map(const std::map<Key, std::vector<uint8_t>> & data)
{
    std::map<Key, pybind11::bytes> result;
    for (const auto & [key, value] : data)
        result.emplace(key, bytes_of(value));
    return result;
}

template <typename Key>
std::map<Key, std::vector<uint8_t>> vector_map(const std::map<Key, pybind11::bytes> & data)
{
    std::map<Key, std::vector<uint8_t>> result;
    for (const auto & [key, value] : data)
        result.emplace(key, vector_of(value));
    return result;
}

// the lambda sees the python objects it creates die under the GIL, the call itself runs without it
template <typename Function>
auto without_gil(Function && function)
{
    pybind11::gil_scoped_release release;
    return function();
}

template <typename T, typename... Options>
void bind_service(pybind11::class_<T, Options...> & binding)
{
    const auto release_gil = pybind11::call_guard<pybind11::gil_scoped_release>();
    binding.def(
               "start",
               [](T & self) { return self.start(); },
               release_gil)
        .def(
            "stop",
            [](T & self) { return self.stop(); },
            release_gil)
        .def("is_running", [](const T & self) { return self.is_running(); })
        .def("is_ready", [](const T & self) { return self.is_ready(); })
        .def(
            "wait_ready",
            [](const T & self, Duration timeout) { return self.wait_ready(timeout); },
            pybind11::arg("timeout"),
            release_gil)
        .def(
            "set_service_wait_stop",
            [](T & self, bool active) { self.set_service_wait_stop(active); },
            pybind11::arg("active"));
}

template <typename T>
pybind11::class_<T, Named, Configurable, SmartNodePtr<T>> bind_node(pybind11::module & bt_module, const char *name)
{
    pybind11::class_<T, Named, Configurable, SmartNodePtr<T>> binding(bt_module, name);
    binding.def(pybind11::init<const std::string &, Node *>(), pybind11::keep_alive<1, 3>())
        .def(pybind11::init<const std::string &>());
    bind_service(binding);
    return binding;
}

void bind_adapter(pybind11::module & bt_module)
{
    pybind11::enum_<bt::AddressType>(bt_module, "AddressType")
        .value("public_address", bt::AddressType::public_address)
        .value("random", bt::AddressType::random)
        .value("unspecified", bt::AddressType::unspecified);

    pybind11::class_<bt::AdapterInfo>(bt_module, "AdapterInfo")
        .def(pybind11::init<>())
        .def_readonly("identifier", &bt::AdapterInfo::identifier)
        .def_readonly("address", &bt::AdapterInfo::address)
        .def_readonly("name", &bt::AdapterInfo::name)
        .def_readonly("alias", &bt::AdapterInfo::alias)
        .def_readonly("powered", &bt::AdapterInfo::powered)
        .def_readonly("discoverable", &bt::AdapterInfo::discoverable)
        .def_readonly("pairable", &bt::AdapterInfo::pairable)
        .def_readonly("discovering", &bt::AdapterInfo::discovering);

    const auto release_gil = pybind11::call_guard<pybind11::gil_scoped_release>();
    bt_module.def("bluetooth_enabled", &bt::bluetooth_enabled, release_gil);
    bt_module.def("list_adapters", &bt::list_adapters, release_gil);
    bt_module.def("set_adapter_powered",
                  &bt::set_adapter_powered,
                  pybind11::arg("index"),
                  pybind11::arg("powered"),
                  release_gil);
    bt_module.def("set_adapter_discoverable",
                  &bt::set_adapter_discoverable,
                  pybind11::arg("index"),
                  pybind11::arg("discoverable"),
                  release_gil);
    bt_module.def("set_adapter_pairable",
                  &bt::set_adapter_pairable,
                  pybind11::arg("index"),
                  pybind11::arg("pairable"),
                  release_gil);
    bt_module.def("set_adapter_alias",
                  &bt::set_adapter_alias,
                  pybind11::arg("index"),
                  pybind11::arg("alias"),
                  release_gil);
}

void bind_scanner(pybind11::module & bt_module)
{
    pybind11::enum_<bt::Transport>(bt_module, "Transport")
        .value("any", bt::Transport::any)
        .value("le", bt::Transport::le)
        .value("classic", bt::Transport::classic);

    pybind11::class_<bt::ScanFilter>(bt_module, "ScanFilter")
        .def(pybind11::init<>())
        .def_readwrite("transport", &bt::ScanFilter::transport)
        .def_readwrite("uuids", &bt::ScanFilter::uuids)
        .def_readwrite("min_rssi", &bt::ScanFilter::min_rssi);

    pybind11::class_<bt::ScanResult>(bt_module, "ScanResult")
        .def(pybind11::init<>())
        .def_readonly("identifier", &bt::ScanResult::identifier)
        .def_readonly("address", &bt::ScanResult::address)
        .def_readonly("address_type", &bt::ScanResult::address_type)
        .def_readonly("rssi", &bt::ScanResult::rssi)
        .def_readonly("tx_power", &bt::ScanResult::tx_power)
        .def_readonly("connectable", &bt::ScanResult::connectable)
        .def_property_readonly("manufacturer_data",
                               [](const bt::ScanResult & self) { return bytes_map(self.manufacturer_data); })
        .def_property_readonly("service_data", [](const bt::ScanResult & self) { return bytes_map(self.service_data); })
        .def_readonly("uuids", &bt::ScanResult::uuids)
        .def_readonly("appearance", &bt::ScanResult::appearance)
        .def_readonly("class_of_device", &bt::ScanResult::class_of_device)
        .def_readonly("timestamp", &bt::ScanResult::timestamp);

    auto scanner = bind_node<bt::Scanner>(bt_module, "Scanner");
    pybind11::enum_<bt::Scanner::Event>(scanner, "Event")
        .value("found", bt::Scanner::Event::found)
        .value("updated", bt::Scanner::Event::updated)
        .value("lost", bt::Scanner::Event::lost);

    scanner.def("set_adapter", &bt::Scanner::set_adapter, pybind11::arg("index"))
        .def("set_filter", &bt::Scanner::set_filter, pybind11::arg("filter"))
        .def("set_lost_timeout", &bt::Scanner::set_lost_timeout, pybind11::arg("timeout"))
        .def("scan_for",
             &bt::Scanner::scan_for,
             pybind11::arg("duration"),
             pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("results", &bt::Scanner::results)
        .def("last_result", &bt::Scanner::last_result)
        .def("last_event", &bt::Scanner::last_event)
        .def("add_observer", &PyUtilApi::PyObservers<bt::Scanner>::add, pybind11::arg("observer"))
        .def("remove_observers", &PyUtilApi::PyObservers<bt::Scanner>::remove_all);
}

void bind_gatt(pybind11::module & bt_module)
{
    pybind11::enum_<bt::WriteType>(bt_module, "WriteType")
        .value("with_response", bt::WriteType::with_response)
        .value("without_response", bt::WriteType::without_response);

    pybind11::class_<bt::GattDescriptor>(bt_module, "GattDescriptor")
        .def(pybind11::init<>())
        .def_readwrite("uuid", &bt::GattDescriptor::uuid)
        .def_readwrite("handle", &bt::GattDescriptor::handle);

    pybind11::class_<bt::GattCharacteristic> characteristic(bt_module, "GattCharacteristic");
    pybind11::enum_<bt::GattCharacteristic::Property>(characteristic, "Property", pybind11::arithmetic())
        .value("broadcast", bt::GattCharacteristic::Property::broadcast)
        .value("read", bt::GattCharacteristic::Property::read)
        .value("write_without_response", bt::GattCharacteristic::Property::write_without_response)
        .value("write", bt::GattCharacteristic::Property::write)
        .value("notify", bt::GattCharacteristic::Property::notify)
        .value("indicate", bt::GattCharacteristic::Property::indicate)
        .value("signed_write", bt::GattCharacteristic::Property::signed_write)
        .value("extended_properties", bt::GattCharacteristic::Property::extended_properties);
    characteristic.def(pybind11::init<>())
        .def_readwrite("uuid", &bt::GattCharacteristic::uuid)
        .def_readwrite("handle", &bt::GattCharacteristic::handle)
        .def_readwrite("properties", &bt::GattCharacteristic::properties)
        .def_readwrite("descriptors", &bt::GattCharacteristic::descriptors);

    pybind11::class_<bt::GattService>(bt_module, "GattService")
        .def(pybind11::init<>())
        .def_readwrite("uuid", &bt::GattService::uuid)
        .def_readwrite("handle", &bt::GattService::handle)
        .def_readwrite("primary", &bt::GattService::primary)
        .def_readwrite("characteristics", &bt::GattService::characteristics);

    pybind11::class_<bt::GattNotification>(bt_module, "GattNotification")
        .def(pybind11::init<>())
        .def_readonly("handle", &bt::GattNotification::handle)
        .def_readonly("uuid", &bt::GattNotification::uuid)
        .def_property_readonly("value", [](const bt::GattNotification & self) { return bytes_of(self.value); })
        .def_readonly("timestamp", &bt::GattNotification::timestamp);
}

void bind_device(pybind11::module & bt_module)
{
    pybind11::class_<bt::DeviceInfo>(bt_module, "DeviceInfo")
        .def(pybind11::init<>())
        .def_readonly("address", &bt::DeviceInfo::address)
        .def_readonly("address_type", &bt::DeviceInfo::address_type)
        .def_readonly("name", &bt::DeviceInfo::name)
        .def_readonly("paired", &bt::DeviceInfo::paired)
        .def_readonly("connected", &bt::DeviceInfo::connected)
        .def_readonly("trusted", &bt::DeviceInfo::trusted)
        .def_readonly("appearance", &bt::DeviceInfo::appearance)
        .def_readonly("class_of_device", &bt::DeviceInfo::class_of_device)
        .def_readonly("uuids", &bt::DeviceInfo::uuids);

    pybind11::class_<bt::PairingRequest> request(bt_module, "PairingRequest");
    pybind11::enum_<bt::PairingRequest::Kind>(request, "Kind")
        .value("confirm", bt::PairingRequest::Kind::confirm)
        .value("confirm_passkey", bt::PairingRequest::Kind::confirm_passkey)
        .value("display_passkey", bt::PairingRequest::Kind::display_passkey)
        .value("enter_passkey", bt::PairingRequest::Kind::enter_passkey)
        .value("display_pin", bt::PairingRequest::Kind::display_pin)
        .value("enter_pin", bt::PairingRequest::Kind::enter_pin);
    request.def(pybind11::init<>())
        .def_readonly("address", &bt::PairingRequest::address)
        .def_readonly("kind", &bt::PairingRequest::kind)
        .def_readonly("passkey", &bt::PairingRequest::passkey)
        .def_readonly("pin", &bt::PairingRequest::pin);

    pybind11::class_<bt::PairingAnswer>(bt_module, "PairingAnswer")
        .def(pybind11::init<>())
        .def_readwrite("accept", &bt::PairingAnswer::accept)
        .def_readwrite("passkey", &bt::PairingAnswer::passkey)
        .def_readwrite("pin", &bt::PairingAnswer::pin);

    auto device = bind_node<bt::Device>(bt_module, "Device");
    pybind11::enum_<bt::Device::Event>(device, "Event")
        .value("connected", bt::Device::Event::connected)
        .value("disconnected", bt::Device::Event::disconnected)
        .value("notification", bt::Device::Event::notification);

    const auto release_gil = pybind11::call_guard<pybind11::gil_scoped_release>();
    device.def("set_address", &bt::Device::set_address, pybind11::arg("address"))
        .def("set_adapter", &bt::Device::set_adapter, pybind11::arg("index"))
        .def(
            "set_pairing_handler",
            [](bt::Device & self, std::optional<pybind11::function> handler) {
                if (handler.has_value() == false)
                {
                    self.set_pairing_handler(nullptr);
                    return;
                }
                // the system agent asks from a backend thread, an answer that fails is a refusal
                self.set_pairing_handler(
                    [callback = PyUtilApi::PyCallback(std::move(*handler))](const bt::PairingRequest & request) {
                        bt::PairingAnswer answer;
                        callback.run([&](pybind11::function & function) {
                            answer = function(request).cast<bt::PairingAnswer>();
                        });
                        return answer;
                    });
            },
            pybind11::arg("handler"))

        .def("info", &bt::Device::info, release_gil)
        .def("pair", &bt::Device::pair, release_gil)
        .def("cancel_pairing", &bt::Device::cancel_pairing, release_gil)
        .def("unpair", &bt::Device::unpair, release_gil)
        .def("set_trusted", &bt::Device::set_trusted, pybind11::arg("trusted"), release_gil)
        .def("services", &bt::Device::services, release_gil)

        .def(
            "read_characteristic",
            [](bt::Device & self, const bt::GattCharacteristic & characteristic) -> Result<pybind11::bytes> {
                auto value = without_gil([&] { return self.read_characteristic(characteristic); });
                SIHD_UNEXPECTED_RETURN(value);
                return bytes_of(*value);
            },
            pybind11::arg("characteristic"))
        .def(
            "write_characteristic",
            [](bt::Device & self,
               const bt::GattCharacteristic & characteristic,
               const pybind11::bytes & value,
               bt::WriteType type) {
                const std::string data = value;
                return without_gil([&] { return self.write_characteristic(characteristic, ArrUByteView(data), type); });
            },
            pybind11::arg("characteristic"),
            pybind11::arg("value"),
            pybind11::arg("type"))
        .def(
            "read_descriptor",
            [](bt::Device & self, const bt::GattDescriptor & descriptor) -> Result<pybind11::bytes> {
                auto value = without_gil([&] { return self.read_descriptor(descriptor); });
                SIHD_UNEXPECTED_RETURN(value);
                return bytes_of(*value);
            },
            pybind11::arg("descriptor"))
        .def(
            "write_descriptor",
            [](bt::Device & self, const bt::GattDescriptor & descriptor, const pybind11::bytes & value) {
                const std::string data = value;
                return without_gil([&] { return self.write_descriptor(descriptor, ArrUByteView(data)); });
            },
            pybind11::arg("descriptor"),
            pybind11::arg("value"))
        .def("subscribe", &bt::Device::subscribe, pybind11::arg("characteristic"), release_gil)
        .def("unsubscribe", &bt::Device::unsubscribe, pybind11::arg("characteristic"), release_gil)
        .def("mtu", &bt::Device::mtu, release_gil)

        .def("last_event", &bt::Device::last_event)
        .def("last_notification", &bt::Device::last_notification)
        .def("add_observer", &PyUtilApi::PyObservers<bt::Device>::add, pybind11::arg("observer"))
        .def("remove_observers", &PyUtilApi::PyObservers<bt::Device>::remove_all);
}

void bind_local_service(pybind11::module & bt_module)
{
    pybind11::class_<bt::LocalCharacteristic>(bt_module, "LocalCharacteristic")
        .def(pybind11::init<>())
        .def_readwrite("uuid", &bt::LocalCharacteristic::uuid)
        .def_readwrite("properties", &bt::LocalCharacteristic::properties)
        .def_property("on_read",
                      nullptr,
                      [](bt::LocalCharacteristic & self, std::optional<pybind11::function> handler) {
                          if (handler.has_value() == false)
                          {
                              self.on_read = nullptr;
                              return;
                          }
                          // the backend asks from its own thread, a failing function reads as an empty value
                          self.on_read = [callback = PyUtilApi::PyCallback(std::move(*handler))] {
                              std::vector<uint8_t> value;
                              callback.run([&](pybind11::function & function) {
                                  value = vector_of(function().cast<pybind11::bytes>());
                              });
                              return value;
                          };
                      })

        .def_property("on_write",
                      nullptr,
                      [](bt::LocalCharacteristic & self, std::optional<pybind11::function> handler) {
                          if (handler.has_value() == false)
                          {
                              self.on_write = nullptr;
                              return;
                          }
                          // a failing function refuses the write
                          self.on_write = [callback = PyUtilApi::PyCallback(std::move(*handler))](
                                              const std::vector<uint8_t> & value) {
                              bool permitted = false;
                              callback.run([&](pybind11::function & function) {
                                  permitted = function(bytes_of(value)).cast<bool>();
                              });
                              return permitted;
                          };
                      });

    pybind11::class_<bt::LocalService>(bt_module, "LocalService")
        .def(pybind11::init<>())
        .def_readwrite("uuid", &bt::LocalService::uuid)
        .def_readwrite("primary", &bt::LocalService::primary)
        .def_readwrite("characteristics", &bt::LocalService::characteristics);

    auto server = bind_node<bt::GattServer>(bt_module, "GattServer");
    server.def("set_adapter", &bt::GattServer::set_adapter, pybind11::arg("index"))
        .def("add_service", &bt::GattServer::add_service, pybind11::arg("service"))
        .def(
            "notify",
            [](bt::GattServer & self,
               const std::string & service_uuid,
               const std::string & characteristic_uuid,
               const pybind11::bytes & value) {
                const std::string data = value;
                return without_gil([&] { return self.notify(service_uuid, characteristic_uuid, ArrUByteView(data)); });
            },
            pybind11::arg("service_uuid"),
            pybind11::arg("characteristic_uuid"),
            pybind11::arg("value"));
}

void bind_advertiser(pybind11::module & bt_module)
{
    pybind11::class_<bt::Advertisement>(bt_module, "Advertisement")
        .def(pybind11::init<>())
        .def_readwrite("local_name", &bt::Advertisement::local_name)
        .def_readwrite("service_uuids", &bt::Advertisement::service_uuids)
        .def_property(
            "manufacturer_data",
            [](const bt::Advertisement & self) { return bytes_map(self.manufacturer_data); },
            [](bt::Advertisement & self, const std::map<uint16_t, pybind11::bytes> & data) {
                self.manufacturer_data = vector_map(data);
            })
        .def_property(
            "service_data",
            [](const bt::Advertisement & self) { return bytes_map(self.service_data); },
            [](bt::Advertisement & self, const std::map<std::string, pybind11::bytes> & data) {
                self.service_data = vector_map(data);
            })
        .def_readwrite("connectable", &bt::Advertisement::connectable);

    auto advertiser = bind_node<bt::Advertiser>(bt_module, "Advertiser");
    advertiser.def("set_adapter", &bt::Advertiser::set_adapter, pybind11::arg("index"))
        .def("set_advertisement", &bt::Advertiser::set_advertisement, pybind11::arg("advertisement"));
}

// a constructor connecting the socket raises the error of the connection, its code kept
std::unique_ptr<bt::Socket> connected(const std::function<Result<void>(bt::Socket &)> & connect)
{
    auto socket = std::make_unique<bt::Socket>();
    auto done = without_gil([&] { return connect(*socket); });
    if (!done)
        raise_error(done.error());
    return socket;
}

void bind_socket(pybind11::module & bt_module)
{
    const auto release_gil = pybind11::call_guard<pybind11::gil_scoped_release>();
    pybind11::class_<bt::Socket>(bt_module, "Socket")
        .def(pybind11::init<>())
        .def(pybind11::init([](const std::string & address, const std::string & uuid) {
                 return connected([&](bt::Socket & socket) { return socket.connect_rfcomm(address, uuid); });
             }),
             pybind11::arg("address"),
             pybind11::arg("uuid"))
        .def(pybind11::init([](const std::string & address, uint8_t channel) {
                 return connected([&](bt::Socket & socket) { return socket.connect_rfcomm_channel(address, channel); });
             }),
             pybind11::arg("address"),
             pybind11::arg("channel"))
        .def(pybind11::init([](const std::string & address, bt::AddressType type, uint16_t psm) {
                 return connected([&](bt::Socket & socket) { return socket.connect_l2cap(address, type, psm); });
             }),
             pybind11::arg("address"),
             pybind11::arg("type"),
             pybind11::arg("psm"))
        .def(pybind11::init(
                 [](uint16_t psm) { return connected([&](bt::Socket & socket) { return socket.listen_l2cap(psm); }); }),
             pybind11::arg("psm"))

        .def("connect_rfcomm",
             &bt::Socket::connect_rfcomm,
             pybind11::arg("address"),
             pybind11::arg("uuid"),
             release_gil)
        .def("connect_rfcomm_channel",
             &bt::Socket::connect_rfcomm_channel,
             pybind11::arg("address"),
             pybind11::arg("channel"),
             release_gil)
        .def("connect_l2cap",
             &bt::Socket::connect_l2cap,
             pybind11::arg("address"),
             pybind11::arg("type"),
             pybind11::arg("psm"),
             release_gil)
        .def("listen_rfcomm", &bt::Socket::listen_rfcomm, pybind11::arg("uuid"), pybind11::arg("name"), release_gil)
        .def("listen_l2cap", &bt::Socket::listen_l2cap, pybind11::arg("psm"), release_gil)

        .def("address", &bt::Socket::address)
        .def("is_open", &bt::Socket::is_open)
        .def("is_listening", &bt::Socket::is_listening)
        .def("psm", &bt::Socket::psm)

        .def(
            "send",
            [](bt::Socket & self, const pybind11::bytes & data) {
                const std::string bytes = data;
                return without_gil([&] { return self.send(ArrUByteView(bytes)); });
            },
            pybind11::arg("data"))
        .def(
            "receive",
            [](bt::Socket & self, size_t size) -> Result<pybind11::bytes> {
                ArrUByte buffer;
                buffer.reserve(size);
                auto received = without_gil([&] { return self.receive(buffer); });
                SIHD_UNEXPECTED_RETURN(received);
                return pybind11::bytes(reinterpret_cast<const char *>(buffer.data()), *received);
            },
            pybind11::arg("size"))
        .def("accept", &bt::Socket::accept, pybind11::arg("timeout"), release_gil)
        .def("close", &bt::Socket::close, release_gil);
}

} // namespace

void PyBtApi::add_bt_api(PyApi::PyModule & pymodule)
{
    auto m_sihd = pymodule.module();
    if (pymodule.load("sihd_util") == false)
        return;

    pybind11::module m_bt = m_sihd.def_submodule("bt", "sihd::bt");

    bind_adapter(m_bt);
    bind_scanner(m_bt);
    bind_gatt(m_bt);
    bind_device(m_bt);
    bind_local_service(m_bt);
    bind_advertiser(m_bt);
    bind_socket(m_bt);
}

namespace
{

void __attribute__((constructor)) premain()
{
    PyApi::add_api("sihd_bt", &PyBtApi::add_bt_api);
}

} // namespace

} // namespace sihd::py
