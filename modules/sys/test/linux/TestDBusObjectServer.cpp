#include <algorithm>
#include <atomic>
#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;

namespace
{

constexpr const char *service = "org.sihd.Test";
constexpr const char *path = "/org/sihd/Test";
constexpr const char *interface_name = "org.sihd.Test.Echo";
constexpr const char *peer = "org.freedesktop.DBus.Peer";
constexpr const char *introspectable = "org.freedesktop.DBus.Introspectable";
constexpr const char *properties = "org.freedesktop.DBus.Properties";
constexpr const char *object_manager = "org.freedesktop.DBus.ObjectManager";

constexpr const char *standard_xml = R"(<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.freedesktop.DBus.Peer">
    <method name="Ping"/>
    <method name="GetMachineId">
      <arg name="machine_uuid" type="s" direction="out"/>
    </method>
  </interface>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect">
      <arg name="xml_data" type="s" direction="out"/>
    </method>
  </interface>
)";

constexpr const char *object_manager_xml = R"(  <interface name="org.freedesktop.DBus.ObjectManager">
    <method name="GetManagedObjects">
      <arg name="objects" type="a{oa{sa{sv}}}" direction="out"/>
    </method>
    <signal name="InterfacesAdded">
      <arg name="object_path" type="o"/>
      <arg name="interfaces_and_properties" type="a{sa{sv}}"/>
    </signal>
    <signal name="InterfacesRemoved">
      <arg name="object_path" type="o"/>
      <arg name="interfaces" type="as"/>
    </signal>
  </interface>
)";

constexpr const char *properties_xml = R"(  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get">
      <arg name="interface_name" type="s" direction="in"/>
      <arg name="property_name" type="s" direction="in"/>
      <arg name="value" type="v" direction="out"/>
    </method>
    <method name="GetAll">
      <arg name="interface_name" type="s" direction="in"/>
      <arg name="properties" type="a{sv}" direction="out"/>
    </method>
    <method name="Set">
      <arg name="interface_name" type="s" direction="in"/>
      <arg name="property_name" type="s" direction="in"/>
      <arg name="value" type="v" direction="in"/>
    </method>
    <signal name="PropertiesChanged">
      <arg name="interface_name" type="s"/>
      <arg name="changed_properties" type="a{sv}"/>
      <arg name="invalidated_properties" type="as"/>
    </signal>
  </interface>
)";

struct BusctlRun
{
        int code = -1;
        std::string output;
};

struct Refused
{
        std::string object;
        std::string interface;
        std::string member;
        ErrorCode code;
        std::string message;
};

Result<void> answer_echo(ObjectServer::Call & call)
{
    auto text = call.argument<std::string>();
    SIHD_UNEXPECTED_RETURN(text);
    return call.reply(*text);
}

ObjectServer::Method echo_method()
{
    return ObjectServer::Method {
        .name = "Echo",
        .in = {{"text", "s"}},
        .out = {{"echoed", "s"}},
        .handler = answer_echo,
    };
}

ObjectServer::Method plain_method(const std::string & name, ObjectServer::MethodHandler handler)
{
    return ObjectServer::Method {.name = name, .in = {}, .out = {}, .handler = std::move(handler)};
}

ObjectServer::Method failing_method(const std::string & name, ErrorCode code)
{
    return plain_method(name, [code](ObjectServer::Call &) -> Result<void> {
        return std::unexpected(Error(code, "it failed"));
    });
}

std::string text_of(const Result<Message> & reply)
{
    return reply.has_value() ? Iter(*reply).get<std::string>().value_or("") : "error: " + reply.error().message;
}

std::map<std::string, std::string> signatures_of(const Message & reply)
{
    std::map<std::string, std::string> signatures;
    (void)Iter(reply).for_each_entry<std::string>([&signatures](const std::string & key, Iter & value) -> Result<void> {
        signatures.emplace(key, value.signature());
        return {};
    });
    return signatures;
}

bool carries_property(const Message & changed, const std::string & name)
{
    return changed.changed_property(interface_name, name).value_or(std::nullopt).has_value();
}

} // namespace

class TestDBusObjectServer: public ::testing::Test
{
    protected:
        TestDBusObjectServer() { LoggerManager::stream(); }

        virtual ~TestDBusObjectServer() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!Connection::supported)
            {
                GTEST_SKIP() << "dbus not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto server_opened = _server.open(_bus.address());
            ASSERT_TRUE(server_opened.has_value()) << server_opened.error().message;
            auto client_opened = _client.open(_bus.address());
            ASSERT_TRUE(client_opened.has_value()) << client_opened.error().message;
            ASSERT_TRUE(_server.request_name(service).has_value());
        }

        Result<void> serve_next()
        {
            auto call = receive_accepted(_server, [](const Message & message) {
                return message.type() == Message::Type::method_call;
            });
            if (!call.has_value())
                return std::unexpected(Error(ErrorCode::timeout, "no method call"));
            auto handled = _objects.handle(std::move(*call));
            SIHD_UNEXPECTED_RETURN(handled);
            return {};
        }

        Result<Message> exchange(const Message & request)
        {
            auto sent = request.send(_client);
            SIHD_UNEXPECTED_RETURN(sent);
            auto served = this->serve_next();
            SIHD_UNEXPECTED_RETURN(served);
            auto reply = receive_reply(_client);
            if (!reply.has_value())
                return std::unexpected(Error(ErrorCode::timeout, "no reply"));
            if (reply->type() == Message::Type::error)
                return std::unexpected(reply->to_error());
            return std::move(*reply);
        }

        Result<Message>
            call_member(const std::string & object, const std::string & interface, const std::string & member)
        {
            Message request;
            auto built = request.method_call(service, object, interface, member);
            SIHD_UNEXPECTED_RETURN(built);
            return this->exchange(request);
        }

        Result<Message> call_echo(const std::string & interface)
        {
            Message request;
            auto built = request.method_call(service, path, interface, "Echo");
            SIHD_UNEXPECTED_RETURN(built);
            auto added = request.add("hello");
            SIHD_UNEXPECTED_RETURN(added);
            return this->exchange(request);
        }

        Result<Message> call_get(const std::string & name)
        {
            Message request;
            auto built = request.properties_get(service, path, interface_name, name);
            SIHD_UNEXPECTED_RETURN(built);
            return this->exchange(request);
        }

        Result<Message> call_set_string(const std::string & name, const std::string & value)
        {
            Message request;
            auto built = request.method_call(service, path, properties, "Set");
            SIHD_UNEXPECTED_RETURN(built);
            Appender body(request);
            auto interface_added = body.add(interface_name);
            SIHD_UNEXPECTED_RETURN(interface_added);
            auto name_added = body.add(name);
            SIHD_UNEXPECTED_RETURN(name_added);

            auto variant = body.open_variant("s");
            SIHD_UNEXPECTED_RETURN(variant);
            auto value_added = variant->add(value);
            SIHD_UNEXPECTED_RETURN(value_added);
            auto closed = body.close(std::move(*variant));
            SIHD_UNEXPECTED_RETURN(closed);
            return this->exchange(request);
        }

        ObjectServer::Property name_property()
        {
            return ObjectServer::Property {
                .name = "Name",
                .signature = "s",
                .get = [this](Appender & value) { return value.add(_name); },
                .set = [this](Iter & value) -> Result<void> {
                    auto text = value.get<std::string>();
                    SIHD_UNEXPECTED_RETURN(text);
                    if (*text == "refused")
                        return std::unexpected(Error(ErrorCode::permission_denied, "name refused"));
                    _name = std::move(*text);
                    return {};
                },
            };
        }

        static ObjectServer::Property version_property()
        {
            return ObjectServer::Property::read_only<uint32_t>("Version", [] { return uint32_t {3}; });
        }

        BusctlRun run_busctl(const std::vector<std::string> & arguments)
        {
            BusctlRun run;
            sihd::sys::Process busctl;
            busctl.add_argv(std::vector<std::string> {"busctl", "--address=" + _bus.address(), "--no-pager"});
            busctl.add_argv(arguments);
            busctl.stdout_to(run.output);
            if (busctl.execute() && busctl.wait_any().value_or(false) && busctl.terminate())
                run.code = busctl.return_code();
            return run;
        }

        void add_test_object(std::vector<ObjectServer::Method> methods, std::vector<ObjectServer::Property> members)
        {
            ObjectServer::Interface interface {
                .name = interface_name,
                .methods = std::move(methods),
                .properties = std::move(members),
                .signals = {},
            };
            auto added = _objects.add_object(path, {std::move(interface)});
            ASSERT_TRUE(added.has_value()) << added.error().message;
        }

        void add_property_object() { this->add_test_object({}, {this->name_property(), version_property()}); }

        void add_echo_object() { this->add_test_object({echo_method()}, {}); }

        PrivateDBus _bus;
        Connection _server;
        Connection _client;
        ObjectServer _objects {_server};
        std::string _name = "initial";
};

TEST_F(TestDBusObjectServer, test_method_answers)
{
    this->add_echo_object();

    EXPECT_EQ(text_of(this->call_echo(interface_name)), "hello");
}

TEST_F(TestDBusObjectServer, test_call_refusals)
{
    const std::vector<Refused> refusals = {
        {"/nowhere",
         interface_name,
         "Echo",
         ErrorCode::io_error,
         "org.freedesktop.DBus.Error.UnknownObject: no object at /nowhere"},
        {path,
         "org.sihd.Nothing",
         "Echo",
         ErrorCode::io_error,
         "org.freedesktop.DBus.Error.UnknownInterface: no interface org.sihd.Nothing on /org/sihd/Test"},
        {path,
         interface_name,
         "Nothing",
         ErrorCode::io_error,
         "org.freedesktop.DBus.Error.UnknownMethod: no method Nothing on /org/sihd/Test"},
        {path,
         interface_name,
         "Echo",
         ErrorCode::invalid_argument,
         "org.freedesktop.DBus.Error.InvalidArgs: expected arguments 's', got ''"},
    };
    this->add_echo_object();

    for (const Refused & refused : refusals)
    {
        auto reply = this->call_member(refused.object, refused.interface, refused.member);

        ASSERT_FALSE(reply.has_value()) << refused.message;
        EXPECT_EQ(reply.error().code, refused.code);
        EXPECT_EQ(reply.error().message, refused.message);
    }
}

TEST_F(TestDBusObjectServer, test_method_without_interface)
{
    this->add_echo_object();
    ObjectServer::Interface first {.name = "org.sihd.First",
                                   .methods = {echo_method()},
                                   .properties = {},
                                   .signals = {}};
    ObjectServer::Interface second {.name = "org.sihd.Second",
                                    .methods = {echo_method()},
                                    .properties = {},
                                    .signals = {}};
    auto added = _objects.add_object("/org/sihd/Both", {std::move(first), std::move(second)});
    ASSERT_TRUE(added.has_value()) << added.error().message;

    auto unique = this->call_echo("");
    Message ambiguous_request;
    auto built = ambiguous_request.method_call(service, "/org/sihd/Both", "", "Echo");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(ambiguous_request.add("hello").has_value());
    auto ambiguous = this->exchange(ambiguous_request);

    EXPECT_EQ(text_of(unique), "hello");
    EXPECT_EQ(error_of(ambiguous), "org.freedesktop.DBus.Error.UnknownMethod: no method Echo on /org/sihd/Both");
}

TEST_F(TestDBusObjectServer, test_handler_success_sends_empty_reply)
{
    this->add_test_object({plain_method("Quiet", [](ObjectServer::Call &) -> Result<void> { return {}; })}, {});

    auto reply = this->call_member(path, interface_name, "Quiet");

    ASSERT_TRUE(reply.has_value()) << reply.error().message;
    EXPECT_EQ(reply->signature(), "");
}

TEST_F(TestDBusObjectServer, test_handler_error_mapping)
{
    this->add_test_object({failing_method("Invalid", ErrorCode::invalid_argument),
                           failing_method("Denied", ErrorCode::permission_denied),
                           failing_method("Unsupported", ErrorCode::not_supported),
                           failing_method("Late", ErrorCode::timeout),
                           failing_method("Broken", ErrorCode::io_error)},
                          {});

    const std::vector<std::tuple<std::string, ErrorCode, std::string>> mapped = {
        {"Invalid", ErrorCode::invalid_argument, "InvalidArgs"},
        {"Denied", ErrorCode::permission_denied, "AccessDenied"},
        {"Unsupported", ErrorCode::not_supported, "NotSupported"},
        {"Late", ErrorCode::timeout, "TimedOut"},
        {"Broken", ErrorCode::io_error, "Failed"},
    };

    for (const auto & [member, code, name] : mapped)
    {
        auto reply = this->call_member(path, interface_name, member);

        ASSERT_FALSE(reply.has_value()) << member;
        EXPECT_EQ(reply.error().code, code);
        EXPECT_EQ(reply.error().message, "org.freedesktop.DBus.Error." + name + ": it failed");
    }
}

TEST_F(TestDBusObjectServer, test_no_reply_expected)
{
    this->add_echo_object();
    Message request;
    auto built = request.method_call(service, path, interface_name, "Echo");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(request.add("hello").has_value());
    ASSERT_TRUE(request.set_no_reply().has_value());

    ASSERT_TRUE(request.send(_client).has_value());
    auto served = this->serve_next();
    auto ping = this->call_member("/", peer, "Ping");

    EXPECT_TRUE(served.has_value());
    ASSERT_TRUE(ping.has_value()) << ping.error().message;
    EXPECT_EQ(ping->signature(), "");
}

TEST_F(TestDBusObjectServer, test_second_answer_refused)
{
    Result<void> second;
    auto twice = plain_method("Twice", [&second](ObjectServer::Call & call) -> Result<void> {
        auto reply = call.message().method_return();
        SIHD_UNEXPECTED_RETURN(reply);
        auto first = call.send(*reply);
        SIHD_UNEXPECTED_RETURN(first);
        second = call.send(*reply);
        return {};
    });
    this->add_test_object({twice}, {});

    auto reply = this->call_member(path, interface_name, "Twice");

    ASSERT_TRUE(reply.has_value()) << reply.error().message;
    EXPECT_EQ(code_of(second), ErrorCode::invalid_argument);
    EXPECT_EQ(error_of(second), "dbus method call has no answer left to send");
}

TEST_F(TestDBusObjectServer, test_handler_error_after_answer)
{
    auto late = plain_method("Late", [](ObjectServer::Call & call) -> Result<void> {
        auto reply = call.message().method_return();
        SIHD_UNEXPECTED_RETURN(reply);
        auto sent = call.send(*reply);
        SIHD_UNEXPECTED_RETURN(sent);
        return std::unexpected(Error(ErrorCode::io_error, "cleanup failed"));
    });
    this->add_test_object({late}, {});
    Message request;
    auto built = request.method_call(service, path, interface_name, "Late");
    ASSERT_TRUE(built.has_value()) << built.error().message;

    ASSERT_TRUE(request.send(_client).has_value());
    auto served = this->serve_next();
    auto reply = receive_reply(_client);

    EXPECT_EQ(error_of(served), "cleanup failed");
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->type(), Message::Type::method_return);
}

TEST_F(TestDBusObjectServer, test_deferred_answer)
{
    std::optional<ObjectServer::Call> deferred;
    auto later = plain_method("Later", [&deferred](ObjectServer::Call & call) -> Result<void> {
        deferred.emplace(std::move(call));
        return {};
    });
    this->add_test_object({later}, {});

    Message request;
    auto built = request.method_call(service, path, interface_name, "Later");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(request.send(_client).has_value());
    auto served = this->serve_next();
    ASSERT_TRUE(served.has_value()) << served.error().message;
    ASSERT_TRUE(deferred.has_value());
    EXPECT_TRUE(deferred->pending());

    Result<void> answered = std::unexpected(Error(ErrorCode::none, "not answered"));
    std::jthread worker([this, &deferred, &answered] {
        auto reply = deferred->message().method_return();
        if (reply.has_value())
            answered = deferred->send(*reply);
        (void)_server.interrupt();
    });
    Message waited;
    auto interrupted = waited.receive(_server, Duration(time::sec(5)));
    worker.join();
    auto reply = receive_reply(_client);

    EXPECT_TRUE(answered.has_value());
    EXPECT_FALSE(deferred->pending());
    ASSERT_TRUE(interrupted.has_value());
    EXPECT_FALSE(*interrupted);
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->type(), Message::Type::method_return);
}

TEST_F(TestDBusObjectServer, test_dropped_call_answers_failed)
{
    auto dropped = plain_method("Dropped", [](ObjectServer::Call & call) -> Result<void> {
        ObjectServer::Call taken(std::move(call));
        return {};
    });
    this->add_test_object({dropped}, {});

    auto reply = this->call_member(path, interface_name, "Dropped");

    EXPECT_EQ(error_of(reply), "org.freedesktop.DBus.Error.Failed: dbus method call was dropped without an answer");
}

TEST_F(TestDBusObjectServer, test_gives_back_other_messages)
{
    Message signal;
    auto built = signal.signal(path, interface_name, "Changed");
    ASSERT_TRUE(built.has_value()) << built.error().message;

    auto handled = _objects.handle(std::move(signal));

    ASSERT_TRUE(handled.has_value()) << handled.error().message;
    ASSERT_TRUE(handled->has_value());
    EXPECT_EQ((*handled)->member(), "Changed");
}

TEST_F(TestDBusObjectServer, test_peer)
{
    auto ping = this->call_member("/anywhere", peer, "Ping");
    auto machine_id = this->call_member("/anywhere", peer, "GetMachineId");
    auto unknown = this->call_member("/anywhere", peer, "Nothing");

    ASSERT_TRUE(ping.has_value()) << ping.error().message;
    ASSERT_TRUE(machine_id.has_value()) << machine_id.error().message;
    const std::string id = Iter(*machine_id).get<std::string>().value_or("");
    EXPECT_EQ(id.size(), 32u);
    EXPECT_TRUE(std::ranges::all_of(id, [](unsigned char character) { return std::isxdigit(character) != 0; })) << id;
    EXPECT_EQ(error_of(unknown), "org.freedesktop.DBus.Error.UnknownMethod: no method Nothing on /anywhere");
}

TEST_F(TestDBusObjectServer, test_introspect_object)
{
    ObjectServer::Interface interface {
        .name = interface_name,
        .methods = {echo_method()},
        .properties = {this->name_property(),
                       version_property(),
                       ObjectServer::Property {
                           .name = "Quiet",
                           .signature = "as",
                           .get = [](Appender & value) { return value.add_array(std::vector<std::string> {}); },
                           .set = [](Iter &) -> Result<void> { return {}; },
                           .emits_changed = false,
                       }},
        .signals = {ObjectServer::Signal {.name = "Changed", .args = {{"value", "i"}, {"", "s"}}}},
    };
    ASSERT_TRUE(_objects.add_object(path, {std::move(interface)}).has_value());
    ASSERT_TRUE(_objects.add_object("/org/sihd/Test/child/deep", {}).has_value());

    auto reply = this->call_member(path, introspectable, "Introspect");

    const std::string expected = std::string(standard_xml) + properties_xml + R"(  <interface name="org.sihd.Test.Echo">
    <method name="Echo">
      <arg name="text" type="s" direction="in"/>
      <arg name="echoed" type="s" direction="out"/>
    </method>
    <signal name="Changed">
      <arg name="value" type="i"/>
      <arg type="s"/>
    </signal>
    <property name="Name" type="s" access="readwrite"/>
    <property name="Version" type="u" access="read"/>
    <property name="Quiet" type="as" access="readwrite">
      <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false"/>
    </property>
  </interface>
  <node name="child"/>
</node>
)";
    EXPECT_EQ(text_of(reply), expected);
}

TEST_F(TestDBusObjectServer, test_introspect_parent_nodes)
{
    this->add_echo_object();
    ASSERT_TRUE(_objects.add_object("/org/other", {}).has_value());

    auto root = this->call_member("/", introspectable, "Introspect");
    auto parent = this->call_member("/org/sihd", introspectable, "Introspect");
    auto parent_method = this->call_member("/org/sihd", interface_name, "Echo");

    EXPECT_EQ(text_of(root), std::string(standard_xml) + "  <node name=\"org\"/>\n</node>\n");
    EXPECT_EQ(text_of(parent), std::string(standard_xml) + "  <node name=\"Test\"/>\n</node>\n");
    EXPECT_EQ(error_of(parent_method), "org.freedesktop.DBus.Error.UnknownObject: no object at /org/sihd");
}

TEST_F(TestDBusObjectServer, test_introspect_object_manager)
{
    ASSERT_TRUE(_objects.add_object_manager("/org/sihd").has_value());

    auto reply = this->call_member("/org/sihd", introspectable, "Introspect");

    EXPECT_EQ(text_of(reply), std::string(standard_xml) + object_manager_xml + "</node>\n");
}

TEST_F(TestDBusObjectServer, test_add_object_refusals)
{
    const std::vector<std::pair<ObjectServer::Interface, std::string>> refused = {
        {ObjectServer::Interface {.name = "no dots", .methods = {}, .properties = {}, .signals = {}},
         "invalid dbus interface: no dots"},
        {ObjectServer::Interface {.name = peer, .methods = {}, .properties = {}, .signals = {}},
         "dbus interface org.freedesktop.DBus.Peer is answered by the server"},
        {ObjectServer::Interface {.name = "org.sihd.A",
                                  .methods = {echo_method(), echo_method()},
                                  .properties = {},
                                  .signals = {}},
         "dbus interface org.sihd.A repeats a member"},
        {ObjectServer::Interface {
             .name = "org.sihd.A",
             .methods = {ObjectServer::Method {.name = "no-dash", .in = {}, .out = {}, .handler = answer_echo}},
             .properties = {},
             .signals = {}},
         "invalid dbus member: no-dash"},
        {ObjectServer::Interface {
             .name = "org.sihd.A",
             .methods = {ObjectServer::Method {.name = "Empty", .in = {}, .out = {}, .handler = {}}},
             .properties = {},
             .signals = {}},
         "dbus method Empty has no handler"},
        {ObjectServer::Interface {
             .name = "org.sihd.A",
             .methods = {ObjectServer::Method {.name = "Bad", .in = {{"x", "ss"}}, .out = {}, .handler = answer_echo}},
             .properties = {},
             .signals = {}},
         "invalid dbus signature: ss"},
        {ObjectServer::Interface {.name = "org.sihd.A",
                                  .methods = {},
                                  .properties = {},
                                  .signals = {ObjectServer::Signal {.name = "Bad", .args = {{"not<xml>", "s"}}}}},
         "invalid dbus argument name: not<xml>"},
        {ObjectServer::Interface {
             .name = "org.sihd.A",
             .methods = {},
             .properties = {ObjectServer::Property {.name = "Nothing", .signature = "s", .get = {}, .set = {}}},
             .signals = {}},
         "dbus property Nothing has no getter nor setter"},
    };

    for (const auto & [interface, message] : refused)
    {
        auto added = _objects.add_object(path, {interface});

        ASSERT_FALSE(added.has_value()) << message;
        EXPECT_EQ(added.error().code, ErrorCode::invalid_argument);
        EXPECT_EQ(added.error().message, message);
    }

    EXPECT_EQ(error_of(_objects.add_object("not a path", {})), "invalid dbus path: not a path");

    this->add_echo_object();
    auto taken = _objects.add_object(path, {});
    EXPECT_EQ(code_of(taken), ErrorCode::already_exists);
    EXPECT_EQ(error_of(taken), "dbus object /org/sihd/Test already exists");
}

TEST_F(TestDBusObjectServer, test_remove_object)
{
    this->add_echo_object();

    auto removed = _objects.remove_object(path);
    auto removed_again = _objects.remove_object(path);
    auto reply = this->call_echo(interface_name);

    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(code_of(removed_again), ErrorCode::not_found);
    EXPECT_EQ(error_of(removed_again), "no dbus object at /org/sihd/Test");
    EXPECT_EQ(error_of(reply), "org.freedesktop.DBus.Error.UnknownObject: no object at /org/sihd/Test");
}

TEST_F(TestDBusObjectServer, test_handler_removes_its_object)
{
    auto quit = plain_method("Quit", [this](ObjectServer::Call &) { return _objects.remove_object(path); });
    this->add_test_object({quit}, {});

    auto quitted = this->call_member(path, interface_name, "Quit");
    auto gone = this->call_member(path, interface_name, "Quit");

    ASSERT_TRUE(quitted.has_value()) << quitted.error().message;
    EXPECT_EQ(error_of(gone), "org.freedesktop.DBus.Error.UnknownObject: no object at /org/sihd/Test");
}

TEST_F(TestDBusObjectServer, test_property_get)
{
    this->add_property_object();

    auto name = this->call_get("Name");
    auto version = this->call_get("Version");
    auto unknown = this->call_get("Nothing");

    ASSERT_TRUE(name.has_value()) << name.error().message;
    auto name_value = Iter(*name).recurse();
    ASSERT_TRUE(name_value.has_value());
    EXPECT_EQ(name_value->get<std::string>().value_or(""), "initial");
    ASSERT_TRUE(version.has_value()) << version.error().message;
    auto version_value = Iter(*version).recurse();
    ASSERT_TRUE(version_value.has_value());
    EXPECT_EQ(version_value->get<uint32_t>().value_or(0), 3u);
    EXPECT_EQ(error_of(unknown),
              "org.freedesktop.DBus.Error.UnknownProperty: no property Nothing on org.sihd.Test.Echo");
}

TEST_F(TestDBusObjectServer, test_property_get_all)
{
    this->add_property_object();
    Message request;
    auto built = request.properties_get_all(service, path, interface_name);
    ASSERT_TRUE(built.has_value()) << built.error().message;
    Message unknown_request;
    auto unknown_built = unknown_request.properties_get_all(service, path, "org.sihd.Nothing");
    ASSERT_TRUE(unknown_built.has_value()) << unknown_built.error().message;

    auto reply = this->exchange(request);
    auto unknown = this->exchange(unknown_request);

    ASSERT_TRUE(reply.has_value()) << reply.error().message;
    EXPECT_EQ(signatures_of(*reply), (std::map<std::string, std::string> {{"Name", "s"}, {"Version", "u"}}));
    EXPECT_EQ(error_of(unknown),
              "org.freedesktop.DBus.Error.UnknownInterface: no interface org.sihd.Nothing on /org/sihd/Test");
}

TEST_F(TestDBusObjectServer, test_property_set_emits_changed)
{
    this->add_property_object();
    ASSERT_TRUE(_client.add_match(properties_changed_rule(service, path, interface_name)).has_value());

    auto stored = this->call_set_string("Name", "renamed");
    auto changed = receive_member(_client, "PropertiesChanged");

    ASSERT_TRUE(stored.has_value()) << stored.error().message;
    EXPECT_EQ(_name, "renamed");
    ASSERT_TRUE(changed.has_value());
    auto value = changed->changed_property(interface_name, "Name");
    ASSERT_TRUE(value.has_value()) << value.error().message;
    ASSERT_TRUE(value->has_value());
    EXPECT_EQ((*value)->get<std::string>().value_or(""), "renamed");
}

TEST_F(TestDBusObjectServer, test_property_set_refusals)
{
    this->add_property_object();

    auto read_only = this->call_set_string("Version", "1");
    auto unknown = this->call_set_string("Nothing", "1");
    auto refused = this->call_set_string("Name", "refused");

    Message bad_type;
    auto built = bad_type.method_call(service, path, properties, "Set");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    Appender body(bad_type);
    ASSERT_TRUE(body.add(interface_name).has_value());
    ASSERT_TRUE(body.add("Name").has_value());
    auto variant = body.open_variant("u");
    ASSERT_TRUE(variant.has_value());
    ASSERT_TRUE(variant->add(uint32_t {1}).has_value());
    ASSERT_TRUE(body.close(std::move(*variant)).has_value());
    auto mismatch = this->exchange(bad_type);

    EXPECT_EQ(error_of(read_only), "org.freedesktop.DBus.Error.PropertyReadOnly: property Version is read only");
    EXPECT_EQ(error_of(unknown),
              "org.freedesktop.DBus.Error.UnknownProperty: no property Nothing on org.sihd.Test.Echo");
    EXPECT_EQ(error_of(refused), "org.freedesktop.DBus.Error.AccessDenied: name refused");
    EXPECT_EQ(error_of(mismatch), "org.freedesktop.DBus.Error.InvalidArgs: property Name is 's', got 'u'");
    EXPECT_EQ(_name, "initial");
}

TEST_F(TestDBusObjectServer, test_setter_removes_its_object)
{
    ObjectServer::Property leaving = this->name_property();
    leaving.set = [this](Iter &) -> Result<void> {
        return _objects.remove_object(path);
    };
    this->add_test_object({}, {std::move(leaving)});

    auto stored = this->call_set_string("Name", "x");
    auto gone = this->call_get("Name");

    ASSERT_TRUE(stored.has_value()) << stored.error().message;
    EXPECT_EQ(error_of(gone), "org.freedesktop.DBus.Error.UnknownObject: no object at /org/sihd/Test");
}

TEST_F(TestDBusObjectServer, test_getter_cannot_change_objects)
{
    ObjectServer::Property leaving = version_property();
    leaving.get = [this](Appender &) -> Result<void> {
        return _objects.remove_object(path);
    };
    ObjectServer::Property growing = version_property();
    growing.name = "Growing";
    growing.get = [this](Appender &) -> Result<void> {
        return _objects.add_object("/org/sihd/Other", {});
    };
    this->add_test_object({}, {std::move(leaving), std::move(growing)});
    Message get_all;
    auto built = get_all.properties_get_all(service, path, interface_name);
    ASSERT_TRUE(built.has_value()) << built.error().message;

    auto version = this->call_get("Version");
    auto all = this->exchange(get_all);
    auto growing_reply = this->call_get("Growing");
    auto removed = _objects.remove_object(path);

    const std::string
        refused = "org.freedesktop.DBus.Error.InvalidArgs: dbus objects cannot change while a property getter runs";
    EXPECT_EQ(error_of(version), refused);
    EXPECT_EQ(error_of(all), refused);
    EXPECT_EQ(error_of(growing_reply), refused);
    EXPECT_TRUE(removed.has_value());
}

TEST_F(TestDBusObjectServer, test_bound_property)
{
    this->add_test_object({}, {ObjectServer::Property::bound("Name", _name)});

    auto stored = this->call_set_string("Name", "renamed");
    auto read = this->call_get("Name");

    ASSERT_TRUE(stored.has_value()) << stored.error().message;
    EXPECT_EQ(_name, "renamed");
    ASSERT_TRUE(read.has_value()) << read.error().message;
    auto value = Iter(*read).recurse();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->signature(), "s");
    EXPECT_EQ(value->get<std::string>().value_or(""), "renamed");
}

TEST_F(TestDBusObjectServer, test_call_without_interface_header)
{
    auto declared_get = plain_method("Get", [](ObjectServer::Call & call) { return call.reply<std::string>("mine"); });
    this->add_test_object({declared_get}, {});
    ASSERT_TRUE(_objects.add_object("/org/sihd/Plain", {}).has_value());

    auto ping = this->call_member(path, "", "Ping");
    auto introspect = this->call_member("/org/sihd/Plain", "", "Introspect");
    auto own_method = this->call_member(path, "", "Get");
    auto standard_get = this->call_member("/org/sihd/Plain", "", "Get");
    auto unknown = this->call_member(path, "", "Nothing");

    ASSERT_TRUE(ping.has_value()) << ping.error().message;
    ASSERT_TRUE(introspect.has_value()) << introspect.error().message;
    EXPECT_EQ(text_of(own_method), "mine");
    EXPECT_EQ(error_of(standard_get), "org.freedesktop.DBus.Error.InvalidArgs: expected arguments 'ss', got ''");
    EXPECT_EQ(error_of(unknown), "org.freedesktop.DBus.Error.UnknownMethod: no method Nothing on /org/sihd/Test");
}

TEST_F(TestDBusObjectServer, test_write_only_property)
{
    std::string stored;
    ObjectServer::Property secret {
        .name = "Secret",
        .signature = "s",
        .get = {},
        .set = [&stored](Iter & value) -> Result<void> {
            auto text = value.get<std::string>();
            SIHD_UNEXPECTED_RETURN(text);
            stored = std::move(*text);
            return {};
        },
    };
    this->add_test_object({}, {std::move(secret), version_property()});
    ASSERT_TRUE(_client.add_match(properties_changed_rule(service, path, interface_name)).has_value());
    Message get_all;
    auto built = get_all.properties_get_all(service, path, interface_name);
    ASSERT_TRUE(built.has_value()) << built.error().message;

    auto read = this->call_get("Secret");
    auto all = this->exchange(get_all);
    auto written = this->call_set_string("Secret", "hidden");
    auto refused = _objects.emit_properties_changed(path, interface_name, {"Secret"});
    ASSERT_TRUE(_objects.emit_properties_changed(path, interface_name, {"Version"}).has_value());
    auto changed = receive_member(_client, "PropertiesChanged");

    EXPECT_EQ(error_of(read), "org.freedesktop.DBus.Error.InvalidArgs: property Secret is write only");
    ASSERT_TRUE(all.has_value()) << all.error().message;
    EXPECT_EQ(signatures_of(*all), (std::map<std::string, std::string> {{"Version", "u"}}));

    ASSERT_TRUE(written.has_value()) << written.error().message;
    EXPECT_EQ(stored, "hidden");
    EXPECT_EQ(error_of(refused), "no readable dbus property Secret on org.sihd.Test.Echo");
    ASSERT_TRUE(changed.has_value());
    EXPECT_FALSE(carries_property(*changed, "Secret"));
}

TEST_F(TestDBusObjectServer, test_property_set_without_changed_signal)
{
    ObjectServer::Property quiet = this->name_property();
    quiet.emits_changed = false;
    this->add_test_object({}, {std::move(quiet), version_property()});
    ASSERT_TRUE(_client.add_match(properties_changed_rule(service, path, interface_name)).has_value());

    auto stored = this->call_set_string("Name", "renamed");
    ASSERT_TRUE(_objects.emit_properties_changed(path, interface_name, {"Version"}).has_value());
    auto changed = receive_member(_client, "PropertiesChanged");

    ASSERT_TRUE(stored.has_value()) << stored.error().message;
    EXPECT_EQ(_name, "renamed");
    ASSERT_TRUE(changed.has_value());
    EXPECT_FALSE(carries_property(*changed, "Name"));
}

TEST_F(TestDBusObjectServer, test_emit_properties_changed)
{
    this->add_property_object();
    ASSERT_TRUE(_client.add_match(properties_changed_rule(service, path, interface_name)).has_value());
    _name = "updated";

    auto emitted = _objects.emit_properties_changed(path, interface_name, {"Name", "Version"});
    auto no_object = _objects.emit_properties_changed("/nowhere", interface_name, {"Name"});
    auto no_interface = _objects.emit_properties_changed(path, "org.sihd.Nothing", {"Name"});
    auto no_property = _objects.emit_properties_changed(path, interface_name, {"Nothing"});
    auto changed = receive_member(_client, "PropertiesChanged");

    EXPECT_TRUE(emitted.has_value());
    ASSERT_TRUE(changed.has_value());
    auto version = changed->changed_property(interface_name, "Version");
    ASSERT_TRUE(version.has_value()) << version.error().message;
    ASSERT_TRUE(version->has_value());
    EXPECT_EQ((*version)->get<uint32_t>().value_or(0), 3u);
    EXPECT_EQ(error_of(no_object), "no dbus object at /nowhere");
    EXPECT_EQ(error_of(no_interface), "no dbus interface org.sihd.Nothing on /org/sihd/Test");
    EXPECT_EQ(error_of(no_property), "no readable dbus property Nothing on org.sihd.Test.Echo");
}

TEST_F(TestDBusObjectServer, test_object_manager_objects)
{
    ASSERT_TRUE(_objects.add_object_manager("/org/sihd").has_value());
    auto taken = _objects.add_object_manager("/org/sihd");
    this->add_property_object();
    ASSERT_TRUE(_objects.add_object("/org/other", {}).has_value());

    auto reply = this->call_member("/org/sihd", object_manager, "GetManagedObjects");

    EXPECT_EQ(code_of(taken), ErrorCode::already_exists);
    EXPECT_EQ(error_of(taken), "dbus object manager /org/sihd already exists");
    ASSERT_TRUE(reply.has_value()) << reply.error().message;
    EXPECT_EQ(reply->signature(), "a{oa{sa{sv}}}");

    std::vector<std::string> found;
    auto walked = Iter(*reply).for_each_entry<std::string>(
        [&found](const std::string & object, Iter & interfaces) -> Result<void> {
            return interfaces.for_each_entry<std::string>(
                [&found, &object](const std::string & interface, Iter & values) -> Result<void> {
                    return values.for_each_entry<std::string>(
                        [&found, &object, &interface](const std::string & property, Iter &) -> Result<void> {
                            found.emplace_back(object + " " + interface + " " + property);
                            return {};
                        });
                });
        });
    ASSERT_TRUE(walked.has_value()) << walked.error().message;
    EXPECT_EQ(found,
              (std::vector<std::string> {"/org/sihd/Test org.sihd.Test.Echo Name",
                                         "/org/sihd/Test org.sihd.Test.Echo Version"}));
}

TEST_F(TestDBusObjectServer, test_remove_object_manager)
{
    ASSERT_TRUE(_objects.add_object_manager("/org/sihd").has_value());

    auto removed = _objects.remove_object_manager("/org/sihd");
    auto removed_again = _objects.remove_object_manager("/org/sihd");
    auto reply = this->call_member("/org/sihd", object_manager, "GetManagedObjects");

    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(code_of(removed_again), ErrorCode::not_found);
    EXPECT_EQ(error_of(removed_again), "no dbus object manager at /org/sihd");
    EXPECT_EQ(error_of(reply), "org.freedesktop.DBus.Error.UnknownObject: no object at /org/sihd");
}

TEST_F(TestDBusObjectServer, test_call_answers_the_peer_meanwhile)
{
    const Duration timeout(time::sec(5));
    ObjectServer agent(_client);
    auto added = agent.add_object(path,
                                  {ObjectServer::Interface {
                                      .name = interface_name,
                                      .methods = {echo_method()},
                                      .properties = {},
                                      .signals = {},
                                  }});
    ASSERT_TRUE(added.has_value()) << added.error().message;

    std::string asked;
    std::jthread peer([this, &asked, &timeout] {
        auto pair = receive_member(_server, "Pair");
        ASSERT_TRUE(pair.has_value());

        Message ask;
        auto ask_built = ask.method_call(pair->sender(), path, interface_name, "Echo");
        ASSERT_TRUE(ask_built.has_value()) << ask_built.error().message;
        ASSERT_TRUE(ask.add("hello").has_value());
        auto echoed = ask.call(_server, timeout);
        asked = text_of(echoed);

        auto reply = pair->method_return();
        ASSERT_TRUE(reply.has_value()) << reply.error().message;
        ASSERT_TRUE(reply->add(asked).has_value());
        ASSERT_TRUE(reply->send(_server).has_value());
    });

    Message request;
    auto built = request.method_call(service, path, interface_name, "Pair");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    auto paired = agent.call(request, timeout);
    peer.join();

    EXPECT_EQ(asked, "hello");
    EXPECT_EQ(text_of(paired), "hello");
}

TEST_F(TestDBusObjectServer, test_call_fails_like_its_error_reply)
{
    std::jthread peer([this] {
        auto pair = receive_member(_server, "Pair");
        ASSERT_TRUE(pair.has_value());
        auto refusal = pair->error_return("org.sihd.Error.Rejected", "no");
        ASSERT_TRUE(refusal.has_value()) << refusal.error().message;
        ASSERT_TRUE(refusal->send(_server).has_value());
    });

    ObjectServer agent(_client);
    Message request;
    auto built = request.method_call(service, path, interface_name, "Pair");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    auto reply = agent.call(request, Duration(time::sec(5)));
    peer.join();

    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error().message, "org.sihd.Error.Rejected: no");
}

TEST_F(TestDBusObjectServer, test_call_times_out)
{
    ObjectServer agent(_client);
    Message request;
    auto built = request.method_call(service, path, interface_name, "Pair");
    ASSERT_TRUE(built.has_value()) << built.error().message;

    auto reply = agent.call(request, Duration(time::milli(50)));

    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error().code, ErrorCode::timeout);
}

TEST_F(TestDBusObjectServer, test_object_manager_signals)
{
    ASSERT_TRUE(_objects.add_object_manager("/org/sihd").has_value());
    ASSERT_TRUE(_client.add_match(signal_rule(service, "/org/sihd", object_manager, "InterfacesAdded")).has_value());
    ASSERT_TRUE(_client.add_match(signal_rule(service, "/org/sihd", object_manager, "InterfacesRemoved")).has_value());

    this->add_property_object();
    auto added = receive_member(_client, "InterfacesAdded");
    ASSERT_TRUE(_objects.remove_object(path).has_value());
    auto removed = receive_member(_client, "InterfacesRemoved");

    ASSERT_TRUE(added.has_value());
    EXPECT_EQ(added->signature(), "oa{sa{sv}}");
    Iter added_body(*added);
    EXPECT_EQ(added_body.get<std::string>().value_or(""), path);
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->signature(), "oas");
    Iter removed_body(*removed);
    EXPECT_EQ(removed_body.get<std::string>().value_or(""), path);
    ASSERT_TRUE(removed_body.next());
    EXPECT_EQ(removed_body.get_array<std::string>().value_or(std::vector<std::string> {}),
              (std::vector<std::string> {interface_name}));
}

TEST_F(TestDBusObjectServer, test_busctl_understands_the_service)
{
    if (!sihd::sys::os::exists_in_path("busctl"))
    {
        GTEST_SKIP() << "no busctl";
    }
    this->add_property_object();
    ASSERT_TRUE(_objects.add_object_manager("/").has_value());

    std::atomic<bool> stop = false;
    std::jthread serving([this, &stop] {
        Message message;
        while (!stop)
        {
            auto received = message.receive(_server, Duration(time::sec(5)));
            if (received.has_value() && *received)
                (void)_objects.handle(std::move(message));
        }
    });

    auto introspect = this->run_busctl({"introspect", service, path});
    auto property = this->run_busctl({"get-property", service, path, interface_name, "Name"});
    auto tree = this->run_busctl({"--list", "tree", service});
    auto ping = this->run_busctl({"call", service, "/anywhere", peer, "Ping"});

    stop = true;
    (void)_server.interrupt();
    serving.join();

    EXPECT_EQ(introspect.code, 0) << introspect.output;
    EXPECT_EQ(introspect.output, R"(NAME                                TYPE      SIGNATURE RESULT/VALUE FLAGS
org.freedesktop.DBus.Introspectable interface -         -            -
.Introspect                         method    -         s            -
org.freedesktop.DBus.Peer           interface -         -            -
.GetMachineId                       method    -         s            -
.Ping                               method    -         -            -
org.freedesktop.DBus.Properties     interface -         -            -
.Get                                method    ss        v            -
.GetAll                             method    s         a{sv}        -
.Set                                method    ssv       -            -
.PropertiesChanged                  signal    sa{sv}as  -            -
org.sihd.Test.Echo                  interface -         -            -
.Name                               property  s         "initial"    emits-change writable
.Version                            property  u         3            emits-change
)");
    EXPECT_EQ(property.code, 0) << property.output;
    EXPECT_EQ(property.output, "s \"initial\"\n");
    EXPECT_EQ(tree.code, 0) << tree.output;
    EXPECT_EQ(tree.output, "/\n/org\n/org/sihd\n/org/sihd/Test\n");
    EXPECT_EQ(ping.code, 0) << ping.output;
}

} // namespace test
