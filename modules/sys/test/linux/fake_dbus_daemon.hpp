#ifndef __SIHD_SYS_TEST_FAKE_DBUS_DAEMON_HPP__
#define __SIHD_SYS_TEST_FAKE_DBUS_DAEMON_HPP__

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
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

#include "private_dbus.hpp"

namespace test
{

namespace dbus = sihd::sys::linux_::dbus;

inline const sihd::util::Duration fake_timeout(sihd::util::time::sec(5));

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
            this->publish_objects();
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
            dbus::Message signal;
            auto built = signal.signal(path, interface, member);
            ASSERT_TRUE(built.has_value()) << built.error().message;
            dbus::Appender body(signal);
            for (const FakeValue & value : values)
            {
                auto appended = value.append(body);
                ASSERT_TRUE(appended.has_value()) << appended.error().message;
            }
            ASSERT_TRUE(signal.send(_server).has_value());
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

        // from a handler only: the serving thread owns the objects once serve() ran
        void publish(const std::string & path, const std::string & interface, const FakeEntries & properties)
        {
            for (const auto & [name, value] : properties)
                _objects[path][interface].insert_or_assign(name, value);
            auto added = _object_server.add_object(path, {this->interface_of(path, interface)});
            ASSERT_TRUE(added.has_value()) << added.error().message;
        }

        // from a handler only, like publish()
        void unpublish(const std::string & path)
        {
            _objects.erase(path);
            auto removed = _object_server.remove_object(path);
            ASSERT_TRUE(removed.has_value()) << removed.error().message;
        }

        // the current value, read once stop() joined the serving thread or from a handler
        const FakeValue & property(const std::string & path, const std::string & interface, const std::string & name)
        {
            return _objects[path][interface][name];
        }

    private:
        // a Set stores booleans, strings and unsigned integers: the fake reads them back
        static std::optional<FakeValue> written_value(const std::string & signature, dbus::Iter & value)
        {
            if (signature == "b")
                return boolean(value.get<bool>().value_or(false));
            if (signature == "s")
                return text(value.get<std::string>().value_or(""));
            if (signature == "u")
                return u32(value.get<uint32_t>().value_or(0));
            return std::nullopt;
        }

        dbus::ObjectServer::Interface interface_of(const std::string & path, const std::string & interface)
        {
            std::vector<dbus::ObjectServer::Property> members;
            for (const auto & [name, value] : _objects[path][interface])
            {
                members.push_back({
                    .name = name,
                    .signature = value.signature,
                    .get = [this, path, interface, name](
                               dbus::Appender & to) { return _objects[path][interface][name].append(to); },
                    .set = [this, path, interface, name](dbus::Iter & value) -> sihd::util::Result<void> {
                        FakeValue & stored = _objects[path][interface][name];
                        auto written = written_value(stored.signature, value);
                        if (!written.has_value())
                            return std::unexpected(
                                sihd::util::Error(sihd::util::ErrorCode::not_supported, "read only"));
                        stored = std::move(*written);
                        return {};
                    },
                });
            }
            return {.name = interface, .methods = {}, .properties = std::move(members), .signals = {}};
        }

        void publish_objects()
        {
            for (const auto & [path, interfaces] : _objects)
            {
                std::vector<dbus::ObjectServer::Interface> published;
                for (const auto & [interface, properties] : interfaces)
                    published.push_back(this->interface_of(path, interface));
                auto added = _object_server.add_object(path, std::move(published));
                ASSERT_TRUE(added.has_value()) << added.error().message;
            }
            auto manager_added = _object_server.add_object_manager("/");
            ASSERT_TRUE(manager_added.has_value()) << manager_added.error().message;
        }

        void loop(const std::stop_token & stop)
        {
            dbus::Message message;
            while (!stop.stop_requested())
            {
                auto received = message.receive(_server, sihd::util::Duration(sihd::util::time::sec(1)));
                if (!received.has_value())
                    return;
                if (*received && message.type() == dbus::Message::Type::method_call)
                    this->dispatch(std::move(message));
            }
        }

        // a handler answers its member on any path, without declaring a signature: the server answers the rest
        void dispatch(dbus::Message && call)
        {
            _calls.push_back({.interface = call.interface(), .member = call.member()});
            const auto found = _handlers.find(std::make_pair(call.interface(), call.member()));
            if (found != _handlers.end())
                return found->second(call);
            auto handled = _object_server.handle(std::move(call));
            ASSERT_TRUE(handled.has_value()) << handled.error().message;
        }

        dbus::Connection _server;
        dbus::ObjectServer _object_server {_server};
        std::map<std::string, std::map<std::string, std::map<std::string, FakeValue>>> _objects;
        std::map<std::pair<std::string, std::string>, Handler> _handlers;
        std::vector<Call> _calls;
        std::jthread _thread;
};

} // namespace test

#endif
