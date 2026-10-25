#ifndef __SIHD_WIFI_TEST_FAKE_DAEMON_HPP__
#define __SIHD_WIFI_TEST_FAKE_DAEMON_HPP__

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <private_dbus.hpp>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/env.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/time.hpp>

namespace test
{

namespace dbus = sihd::sys::linux_::dbus;

inline constexpr char fake_interface_name[] = "wlan-test";
inline const sihd::util::Duration fake_timeout(sihd::util::time::sec(5));

inline constexpr char nm_service[] = "org.freedesktop.NetworkManager";
inline constexpr char nm_device[] = "/org/freedesktop/NetworkManager/Devices/3";
inline constexpr char nm_device_interface[] = "org.freedesktop.NetworkManager.Device";
inline constexpr char nm_wireless_interface[] = "org.freedesktop.NetworkManager.Device.Wireless";

inline constexpr char iwd_service[] = "net.connman.iwd";
inline constexpr char iwd_station[] = "/net/connman/iwd/0/4";
inline constexpr char iwd_device_interface[] = "net.connman.iwd.Device";
inline constexpr char iwd_station_interface[] = "net.connman.iwd.Station";

inline constexpr char wpa_service[] = "fi.w1.wpa_supplicant1";
inline constexpr char wpa_interface[] = "/fi/w1/wpa_supplicant1/Interfaces/0";
inline constexpr char wpa_interface_interface[] = "fi.w1.wpa_supplicant1.Interface";

// a dbus value the fake answers with: append must write exactly its signature
struct FakeValue
{
        std::string signature;
        std::function<sihd::util::Result<void>(dbus::Appender &)> append;
};

using FakeEntries = std::vector<std::pair<std::string, FakeValue>>;

inline FakeValue text(const std::string & value)
{
    return {"s", [value](dbus::Appender & to) { return to.add(value); }};
}

inline FakeValue object_path(const std::string & path)
{
    return {"o", [path](dbus::Appender & to) { return to.add_object_path(path); }};
}

inline FakeValue boolean(bool value)
{
    return {"b", [value](dbus::Appender & to) { return to.add(value); }};
}

inline FakeValue u8(uint8_t value)
{
    return {"y", [value](dbus::Appender & to) { return to.add(value); }};
}

inline FakeValue u32(uint32_t value)
{
    return {"u", [value](dbus::Appender & to) { return to.add(value); }};
}

inline FakeValue bytes(const std::vector<uint8_t> & value)
{
    return {"ay", [value](dbus::Appender & to) { return to.add_array(value); }};
}

inline FakeValue bytes(const std::string & value)
{
    return bytes(std::vector<uint8_t>(value.begin(), value.end()));
}

inline FakeValue texts(const std::vector<std::string> & values)
{
    return {"as", [values](dbus::Appender & to) { return to.add_array(values); }};
}

inline FakeValue object_paths(const std::vector<std::string> & paths)
{
    return {"ao", [paths](dbus::Appender & to) -> sihd::util::Result<void> {
                auto array = to.open_array("o");
                SIHD_UNEXPECTED_RETURN(array);
                for (const std::string & path : paths)
                {
                    auto added = array->add_object_path(path);
                    SIHD_UNEXPECTED_RETURN(added);
                }
                return to.close(std::move(*array));
            }};
}

inline sihd::util::Result<void> append_entries(dbus::Appender & to, const FakeEntries & entries)
{
    auto dict = to.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(dict);
    for (const auto & [key, value] : entries)
    {
        auto appended = dict->add_entry(key, value.signature, value.append);
        SIHD_UNEXPECTED_RETURN(appended);
    }
    return to.close(std::move(*dict));
}

inline FakeValue dict(const FakeEntries & entries)
{
    return {"a{sv}", [entries](dbus::Appender & to) { return append_entries(to, entries); }};
}

inline FakeValue profile_settings(const std::vector<std::pair<std::string, FakeEntries>> & groups)
{
    return {"a{sa{sv}}", [groups](dbus::Appender & to) -> sihd::util::Result<void> {
                auto array = to.open_array("{sa{sv}}");
                SIHD_UNEXPECTED_RETURN(array);
                for (const auto & [name, entries] : groups)
                {
                    auto entry = array->open_dict_entry();
                    SIHD_UNEXPECTED_RETURN(entry);
                    auto name_added = entry->add(name);
                    SIHD_UNEXPECTED_RETURN(name_added);
                    auto appended = append_entries(*entry, entries);
                    SIHD_UNEXPECTED_RETURN(appended);
                    auto entry_closed = array->close(std::move(*entry));
                    SIHD_UNEXPECTED_RETURN(entry_closed);
                }
                return to.close(std::move(*array));
            }};
}

// libdbus reads DBUS_SYSTEM_BUS_ADDRESS once per process: every fake must share this bus
inline PrivateDBus *shared_bus()
{
    static std::unique_ptr<PrivateDBus> bus;
    if (bus == nullptr)
    {
        auto started = std::make_unique<PrivateDBus>();
        if (started->start() && sihd::sys::env::set("DBUS_SYSTEM_BUS_ADDRESS", started->address()).has_value())
            bus = std::move(started);
    }
    return bus.get();
}

class FakeDaemon
{
    public:
        using Handler = std::function<void(const dbus::Message & call)>;

        struct Call
        {
                std::string interface;
                std::string member;
        };

        ~FakeDaemon() { this->stop(); }

        static bool available() { return PrivateDBus::available(); }

        bool start(const std::vector<std::string> & names)
        {
            const PrivateDBus *bus = shared_bus();
            if (bus == nullptr || !_server.open(bus->address()))
                return false;
            for (const std::string & name : names)
            {
                if (!_server.request_name(name))
                    return false;
            }
            return true;
        }

        void set_property(const std::string & path,
                          const std::string & interface,
                          const std::string & name,
                          const FakeValue & value)
        {
            _objects[path][interface].insert_or_assign(name, value);
        }

        void on(const std::string & interface, const std::string & member, const Handler & handler)
        {
            _handlers.insert_or_assign(std::make_pair(interface, member), handler);
        }

        // handlers and properties stay untouched by the test thread from here on
        void serve()
        {
            _thread = std::jthread([this](std::stop_token stop) { this->loop(stop); });
        }

        void stop()
        {
            if (!_thread.joinable())
                return;
            _thread.request_stop();
            (void)_server.interrupt();
            _thread.join();
        }

        // read once stop() joined the serving thread
        const std::vector<Call> & calls() const { return _calls; }

        bool called(const std::string & interface, const std::string & member) const
        {
            return std::ranges::any_of(_calls, [&interface, &member](const Call & call) {
                return call.interface == interface && call.member == member;
            });
        }

        void reply(const dbus::Message & call, const std::vector<FakeValue> & values)
        {
            auto answer = call.method_return();
            ASSERT_TRUE(answer.has_value()) << answer.error().message;
            dbus::Appender body(*answer);
            for (const FakeValue & value : values)
            {
                auto appended = value.append(body);
                ASSERT_TRUE(appended.has_value()) << appended.error().message;
            }
            ASSERT_TRUE(answer->send(_server).has_value());
        }

        void fail(const dbus::Message & call, const std::string & name, const std::string & text)
        {
            auto answer = call.error_return(name, text);
            ASSERT_TRUE(answer.has_value()) << answer.error().message;
            ASSERT_TRUE(answer->send(_server).has_value());
        }

        void emit(const std::string & path,
                  const std::string & interface,
                  const std::string & member,
                  const std::vector<FakeValue> & values)
        {
            auto signal = dbus::Message::signal(path, interface, member);
            ASSERT_TRUE(signal.has_value()) << signal.error().message;
            dbus::Appender body(*signal);
            for (const FakeValue & value : values)
            {
                auto appended = value.append(body);
                ASSERT_TRUE(appended.has_value()) << appended.error().message;
            }
            ASSERT_TRUE(signal->send(_server).has_value());
        }

        void emit_properties_changed(const std::string & path,
                                     const std::string & interface,
                                     const FakeEntries & changed)
        {
            this->emit(path,
                       "org.freedesktop.DBus.Properties",
                       "PropertiesChanged",
                       {text(interface), dict(changed), texts({})});
        }

        dbus::Connection & connection() { return _server; }

    private:
        void loop(const std::stop_token & stop)
        {
            while (!stop.stop_requested())
            {
                auto received = dbus::Message::receive(_server, sihd::util::Duration(sihd::util::time::sec(1)));
                if (!received.has_value())
                    return;
                if (received->has_value() && (*received)->type() == dbus::Message::Type::method_call)
                    this->dispatch(**received);
            }
        }

        void dispatch(const dbus::Message & call)
        {
            _calls.push_back({.interface = call.interface(), .member = call.member()});
            if (call.interface() == "org.freedesktop.DBus.Properties" && call.member() == "Get")
                return this->answer_get(call);
            if (call.interface() == "org.freedesktop.DBus.Properties" && call.member() == "GetAll")
                return this->answer_get_all(call);
            if (call.interface() == "org.freedesktop.DBus.ObjectManager" && call.member() == "GetManagedObjects")
                return this->answer_managed_objects(call);
            const auto found = _handlers.find(std::make_pair(call.interface(), call.member()));
            if (found == _handlers.end())
                return this->fail(call, "org.freedesktop.DBus.Error.UnknownMethod", call.member());
            found->second(call);
        }

        void answer_get(const dbus::Message & call)
        {
            dbus::Iter arguments(call);
            const std::string interface = arguments.get<std::string>().value_or("");
            arguments.next();
            const std::string name = arguments.get<std::string>().value_or("");
            const auto object = _objects.find(call.path());
            if (object == _objects.end() || !object->second.contains(interface)
                || !object->second.at(interface).contains(name))
                return this->fail(call, "org.freedesktop.DBus.Error.UnknownProperty", name);
            const FakeValue & value = object->second.at(interface).at(name);
            this->reply(call, {FakeValue {"v", [&value](dbus::Appender & to) -> sihd::util::Result<void> {
                                              auto variant = to.open_variant(value.signature);
                                              SIHD_UNEXPECTED_RETURN(variant);
                                              auto appended = value.append(*variant);
                                              SIHD_UNEXPECTED_RETURN(appended);
                                              return to.close(std::move(*variant));
                                          }}});
        }

        void answer_get_all(const dbus::Message & call)
        {
            const std::string interface = dbus::Iter(call).get<std::string>().value_or("");
            const auto object = _objects.find(call.path());
            if (object == _objects.end() || !object->second.contains(interface))
                return this->fail(call, "org.freedesktop.DBus.Error.UnknownInterface", interface);
            const auto & properties = object->second.at(interface);
            this->reply(call, {dict(FakeEntries(properties.begin(), properties.end()))});
        }

        void answer_managed_objects(const dbus::Message & call)
        {
            this->reply(
                call,
                {FakeValue {"a{oa{sa{sv}}}", [this](dbus::Appender & to) { return this->append_objects(to); }}});
        }

        sihd::util::Result<void> append_objects(dbus::Appender & to)
        {
            auto objects = to.open_array("{oa{sa{sv}}}");
            SIHD_UNEXPECTED_RETURN(objects);
            for (const auto & [path, interfaces] : _objects)
            {
                auto object = objects->open_dict_entry();
                SIHD_UNEXPECTED_RETURN(object);
                auto path_added = object->add_object_path(path);
                SIHD_UNEXPECTED_RETURN(path_added);
                auto interface_list = object->open_array("{sa{sv}}");
                SIHD_UNEXPECTED_RETURN(interface_list);
                for (const auto & [interface, properties] : interfaces)
                {
                    auto entry = interface_list->open_dict_entry();
                    SIHD_UNEXPECTED_RETURN(entry);
                    auto interface_added = entry->add(interface);
                    SIHD_UNEXPECTED_RETURN(interface_added);
                    auto appended = append_entries(*entry, FakeEntries(properties.begin(), properties.end()));
                    SIHD_UNEXPECTED_RETURN(appended);
                    auto entry_closed = interface_list->close(std::move(*entry));
                    SIHD_UNEXPECTED_RETURN(entry_closed);
                }
                auto list_closed = object->close(std::move(*interface_list));
                SIHD_UNEXPECTED_RETURN(list_closed);
                auto object_closed = objects->close(std::move(*object));
                SIHD_UNEXPECTED_RETURN(object_closed);
            }
            return to.close(std::move(*objects));
        }

        dbus::Connection _server;
        std::map<std::string, std::map<std::string, std::map<std::string, FakeValue>>> _objects;
        std::map<std::pair<std::string, std::string>, Handler> _handlers;
        std::vector<Call> _calls;
        std::jthread _thread;
};

// networkmanager managing wlan-test, a wifi device unless told otherwise
inline void serve_network_manager(FakeDaemon & daemon, bool managed)
{
    daemon.set_property(nm_device, nm_device_interface, "Managed", boolean(managed));
    daemon.set_property(nm_device, nm_device_interface, "DeviceType", u32(2));
    daemon.on(nm_service, "GetDeviceByIpIface", [&daemon](const dbus::Message & call) {
        if (dbus::Iter(call).get<std::string>().value_or("") != fake_interface_name)
            return daemon.fail(call, "org.freedesktop.NetworkManager.UnknownDevice", "No such device");
        daemon.reply(call, {object_path(nm_device)});
    });
}

inline void serve_iwd_station(FakeDaemon & daemon, const std::string & state)
{
    daemon.set_property(iwd_station, iwd_device_interface, "Name", text(fake_interface_name));
    daemon.set_property(iwd_station, iwd_station_interface, "State", text(state));
}

inline void serve_wpa_supplicant(FakeDaemon & daemon)
{
    daemon.on(wpa_service, "GetInterface", [&daemon](const dbus::Message & call) {
        if (dbus::Iter(call).get<std::string>().value_or("") != fake_interface_name)
            return daemon.fail(call, "fi.w1.wpa_supplicant1.InterfaceUnknown", "wpa_supplicant knows nothing");
        daemon.reply(call, {object_path(wpa_interface)});
    });
}

} // namespace test

#endif
