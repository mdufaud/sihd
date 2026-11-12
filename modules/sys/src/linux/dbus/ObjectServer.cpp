#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <algorithm>
# include <cctype>
# include <string>
# include <utility>
# include <vector>

# include <fmt/format.h>

# include "Impl.hpp"

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

constexpr ZStringView peer_interface = "org.freedesktop.DBus.Peer";
constexpr ZStringView introspectable_interface = "org.freedesktop.DBus.Introspectable";
constexpr ZStringView object_manager_interface = "org.freedesktop.DBus.ObjectManager";

constexpr const char *peer_xml = R"(  <interface name="org.freedesktop.DBus.Peer">
    <method name="Ping"/>
    <method name="GetMachineId">
      <arg name="machine_uuid" type="s" direction="out"/>
    </method>
  </interface>
)";

constexpr const char *introspectable_xml = R"(  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect">
      <arg name="xml_data" type="s" direction="out"/>
    </method>
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

using Arg = ObjectServer::Arg;
using Call = ObjectServer::Call;
using Interface = ObjectServer::Interface;
using Method = ObjectServer::Method;
using Property = ObjectServer::Property;
using Signal = ObjectServer::Signal;

std::string child_prefix(const std::string & path)
{
    return path == "/" ? path : path + "/";
}

bool is_standard_interface(const std::string & name)
{
    return name == peer_interface || name == introspectable_interface || name == properties_interface
           || name == object_manager_interface;
}

template <typename T>
bool has_duplicate_name(const std::vector<T> & items)
{
    std::set<std::string> names;
    for (const T & item : items)
    {
        if (!names.emplace(item.name).second)
            return true;
    }
    return false;
}

Result<void> validate_args(const std::vector<Arg> & args)
{
    for (const Arg & arg : args)
    {
        if (!dbus_signature_validate_single(arg.signature.c_str(), nullptr))
            return std::unexpected(Error(invalid_argument, "invalid dbus signature: {}", arg.signature));
        const bool valid_name = std::ranges::all_of(arg.name, [](unsigned char character) {
            return std::isalnum(character) != 0 || character == '_';
        });
        if (!valid_name)
            return std::unexpected(Error(invalid_argument, "invalid dbus argument name: {}", arg.name));
    }
    return {};
}

Result<void> validate_member_name(const std::string & name)
{
    if (!dbus_validate_member(name.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus member: {}", name));
    return {};
}

Result<void> validate_interface(const Interface & interface)
{
    if (!dbus_validate_interface(interface.name.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus interface: {}", interface.name));
    if (is_standard_interface(interface.name))
        return std::unexpected(Error(invalid_argument, "dbus interface {} is answered by the server", interface.name));
    if (has_duplicate_name(interface.methods) || has_duplicate_name(interface.properties)
        || has_duplicate_name(interface.signals))
        return std::unexpected(Error(invalid_argument, "dbus interface {} repeats a member", interface.name));

    for (const Method & method : interface.methods)
    {
        auto name_valid = validate_member_name(method.name);
        SIHD_UNEXPECTED_RETURN(name_valid);
        auto in_valid = validate_args(method.in);
        SIHD_UNEXPECTED_RETURN(in_valid);
        auto out_valid = validate_args(method.out);
        SIHD_UNEXPECTED_RETURN(out_valid);
        if (!method.handler)
            return std::unexpected(Error(invalid_argument, "dbus method {} has no handler", method.name));
    }

    for (const Signal & signal : interface.signals)
    {
        auto name_valid = validate_member_name(signal.name);
        SIHD_UNEXPECTED_RETURN(name_valid);
        auto args_valid = validate_args(signal.args);
        SIHD_UNEXPECTED_RETURN(args_valid);
    }

    for (const Property & property : interface.properties)
    {
        auto name_valid = validate_member_name(property.name);
        SIHD_UNEXPECTED_RETURN(name_valid);

        if (!dbus_signature_validate_single(property.signature.c_str(), nullptr))
            return std::unexpected(Error(invalid_argument, "invalid dbus signature: {}", property.signature));
        if (!property.get && !property.set)
            return std::unexpected(Error(invalid_argument, "dbus property {} has no getter nor setter", property.name));
    }
    return {};
}

Result<void> validate_interfaces(const std::vector<Interface> & interfaces)
{
    if (has_duplicate_name(interfaces))
        return std::unexpected(Error(invalid_argument, "dbus object repeats an interface"));
    for (const Interface & interface : interfaces)
    {
        auto valid = validate_interface(interface);
        SIHD_UNEXPECTED_RETURN(valid);
    }
    return {};
}

std::string joined_signature(const std::vector<Arg> & args)
{
    std::string signature;
    for (const Arg & arg : args)
        signature += arg.signature;
    return signature;
}

std::string args_xml(const std::vector<Arg> & args, std::string_view direction)
{
    std::string xml;
    for (const Arg & arg : args)
    {
        xml += "      <arg";
        if (!arg.name.empty())
            xml += fmt::format(" name=\"{}\"", arg.name);
        xml += fmt::format(" type=\"{}\"", arg.signature);
        if (!direction.empty())
            xml += fmt::format(" direction=\"{}\"", direction);
        xml += "/>\n";
    }
    return xml;
}

std::string_view property_access(const Property & property)
{
    if (property.get && property.set)
        return "readwrite";
    return property.get ? "read" : "write";
}

std::string interface_xml(const Interface & interface)
{
    std::string xml = fmt::format("  <interface name=\"{}\">\n", interface.name);
    for (const Method & method : interface.methods)
    {
        xml += fmt::format("    <method name=\"{}\">\n", method.name);
        xml += args_xml(method.in, "in");
        xml += args_xml(method.out, "out");
        xml += "    </method>\n";
    }

    for (const Signal & signal : interface.signals)
    {
        xml += fmt::format("    <signal name=\"{}\">\n", signal.name);
        xml += args_xml(signal.args, "");
        xml += "    </signal>\n";
    }

    for (const Property & property : interface.properties)
    {
        const std::string attributes = fmt::format("name=\"{}\" type=\"{}\" access=\"{}\"",
                                                   property.name,
                                                   property.signature,
                                                   property_access(property));
        if (property.emits_changed)
        {
            xml += fmt::format("    <property {}/>\n", attributes);
            continue;
        }
        xml += fmt::format("    <property {}>\n", attributes);
        xml += "      <annotation name=\"org.freedesktop.DBus.Property.EmitsChangedSignal\" value=\"false\"/>\n";
        xml += "    </property>\n";
    }
    xml += "  </interface>\n";
    return xml;
}

const Interface *find_interface(const std::vector<Interface> & interfaces, const std::string & name)
{
    const auto found = std::ranges::find(interfaces, name, &Interface::name);
    return found != interfaces.end() ? &*found : nullptr;
}

const Property *find_property(const Interface & interface, const std::string & name)
{
    const auto found = std::ranges::find(interface.properties, name, &Property::name);
    return found != interface.properties.end() ? &*found : nullptr;
}

bool declares_method(const std::vector<Interface> & interfaces, const std::string & member)
{
    return std::ranges::any_of(interfaces, [&member](const Interface & interface) {
        return std::ranges::find(interface.methods, member, &Method::name) != interface.methods.end();
    });
}

std::string standard_interface_of(const std::string & member)
{
    if (member == "Ping" || member == "GetMachineId")
        return std::string(peer_interface);
    if (member == "Introspect")
        return std::string(introspectable_interface);
    if (member == "Get" || member == "GetAll" || member == "Set")
        return std::string(properties_interface);
    if (member == "GetManagedObjects")
        return std::string(object_manager_interface);
    return {};
}

Error getter_running()
{
    return Error(invalid_argument, "dbus objects cannot change while a property getter runs");
}

Result<void> answer_error(Call & call, ZStringView name, const std::string & text)
{
    auto reply = call.message().error_return(name, text);
    SIHD_UNEXPECTED_RETURN(reply);
    return call.send(*reply);
}

Result<void> answer_unknown_method(Call & call)
{
    return answer_error(call,
                        DBUS_ERROR_UNKNOWN_METHOD,
                        fmt::format("no method {} on {}", call.message().member(), call.message().path()));
}

Result<void> answer_unknown_interface(Call & call, const std::string & interface)
{
    return answer_error(call,
                        DBUS_ERROR_UNKNOWN_INTERFACE,
                        fmt::format("no interface {} on {}", interface, call.message().path()));
}

Result<void> answer_unknown_property(Call & call, const std::string & property, const std::string & interface)
{
    return answer_error(call, DBUS_ERROR_UNKNOWN_PROPERTY, fmt::format("no property {} on {}", property, interface));
}

bool has_arguments(const Call & call, std::string_view expected)
{
    return call.message().signature() == expected;
}

Result<void> answer_invalid_arguments(Call & call, std::string_view expected)
{
    return answer_error(call,
                        DBUS_ERROR_INVALID_ARGS,
                        fmt::format("expected arguments '{}', got '{}'", expected, call.message().signature()));
}

Result<void> serve_peer(Call & call)
{
    const std::string member = call.message().member();
    if (member == "Ping")
        return has_arguments(call, "") ? call.reply() : answer_invalid_arguments(call, "");
    if (member != "GetMachineId")
        return answer_unknown_method(call);
    if (!has_arguments(call, ""))
        return answer_invalid_arguments(call, "");

    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    char *machine_id = dbus_try_get_local_machine_id(&dbus_error);
    if (machine_id == nullptr)
    {
        auto answered = answer_error(call, dbus_error.name, dbus_error.message);
        dbus_error_free(&dbus_error);
        return answered;
    }
    auto answered = call.reply(std::string(machine_id));
    dbus_free(machine_id);
    return answered;
}

Result<void> write_properties(Appender & parent, const Interface & interface)
{
    auto properties = parent.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(properties);
    for (const Property & property : interface.properties)
    {
        if (!property.get)
            continue;
        auto added = properties->add_entry(property.name, property.signature, property.get);
        SIHD_UNEXPECTED_RETURN(added);
    }
    return parent.close(std::move(*properties));
}

Result<void> write_interfaces(Appender & parent, const std::vector<Interface> & interfaces)
{
    auto dict = parent.open_array("{sa{sv}}");
    SIHD_UNEXPECTED_RETURN(dict);
    for (const Interface & interface : interfaces)
    {
        auto entry = dict->open_dict_entry();
        SIHD_UNEXPECTED_RETURN(entry);
        auto name_added = entry->add(interface.name);
        SIHD_UNEXPECTED_RETURN(name_added);
        auto properties_written = write_properties(*entry, interface);
        SIHD_UNEXPECTED_RETURN(properties_written);
        auto entry_closed = dict->close(std::move(*entry));
        SIHD_UNEXPECTED_RETURN(entry_closed);
    }
    return parent.close(std::move(*dict));
}

Result<Message> property_get_reply(const Call & call, const Property & property)
{
    auto reply = call.message().method_return();
    SIHD_UNEXPECTED_RETURN(reply);
    Appender body(*reply);
    auto value = body.open_variant(property.signature);
    SIHD_UNEXPECTED_RETURN(value);
    auto written = property.get(*value);
    SIHD_UNEXPECTED_RETURN(written);
    auto closed = body.close(std::move(*value));
    SIHD_UNEXPECTED_RETURN(closed);
    return reply;
}

Result<Message> all_properties_reply(const Call & call, const Interface & interface)
{
    auto reply = call.message().method_return();
    SIHD_UNEXPECTED_RETURN(reply);
    Appender body(*reply);
    auto written = write_properties(body, interface);
    SIHD_UNEXPECTED_RETURN(written);
    return reply;
}

Result<Message> managed_objects_reply(const Call & call, const std::map<std::string, std::vector<Interface>> & objects)
{
    const std::string manager = call.message().path();
    const std::string prefix = child_prefix(manager);
    auto reply = call.message().method_return();
    SIHD_UNEXPECTED_RETURN(reply);
    Appender body(*reply);
    auto dict = body.open_array("{oa{sa{sv}}}");
    SIHD_UNEXPECTED_RETURN(dict);
    for (const auto & [path, interfaces] : objects)
    {
        if (path == manager || !path.starts_with(prefix))
            continue;

        auto entry = dict->open_dict_entry();
        SIHD_UNEXPECTED_RETURN(entry);
        auto path_added = entry->add_object_path(path);
        SIHD_UNEXPECTED_RETURN(path_added);
        auto interfaces_written = write_interfaces(*entry, interfaces);
        SIHD_UNEXPECTED_RETURN(interfaces_written);
        auto entry_closed = dict->close(std::move(*entry));
        SIHD_UNEXPECTED_RETURN(entry_closed);
    }
    auto dict_closed = body.close(std::move(*dict));
    SIHD_UNEXPECTED_RETURN(dict_closed);
    return reply;
}

void add_child(std::set<std::string> & children, const std::string & path, const std::string & other)
{
    const std::string prefix = child_prefix(path);
    if (other == path || !other.starts_with(prefix))
        return;
    const std::string_view below = std::string_view(other).substr(prefix.size());
    children.emplace(below.substr(0, below.find('/')));
}

Result<void> answer_with(Call & call, Result<Message> && reply)
{
    if (!reply.has_value())
        return call.fail(reply.error());
    return call.send(*reply);
}

} // namespace

# endif

ObjectServer::Call::Call(Token, Connection & connection, Message && message):
    _connection(&connection),
    _message(std::move(message))
{
}

ObjectServer::Call::Call(Call && other) = default;

ObjectServer::Call::~Call()
{
    if (this->pending())
        (void)this->fail(Error(io_error, "dbus method call was dropped without an answer"));
}

const Message & ObjectServer::Call::message() const
{
    return _message;
}

bool ObjectServer::Call::pending() const
{
    return !_answered && _message.type() == Message::Type::method_call;
}

Result<void> ObjectServer::Call::send(const Message & reply)
{
    if (!this->pending())
        return std::unexpected(Error(invalid_argument, "dbus method call has no answer left to send"));
    if (_message.expects_reply())
    {
        auto sent = reply.send(*_connection);
        SIHD_UNEXPECTED_RETURN(sent);
    }
    _answered = true;
    return {};
}

template <BasicType T>
Result<T> ObjectServer::Call::argument() const
{
    return Iter(_message).get<T>();
}

Result<void> ObjectServer::Call::reply()
{
    auto answer = _message.method_return();
    SIHD_UNEXPECTED_RETURN(answer);
    return this->send(*answer);
}

template <BasicType T>
Result<void> ObjectServer::Call::reply(const T & value)
{
    auto answer = _message.method_return();
    SIHD_UNEXPECTED_RETURN(answer);
    auto added = answer->add(value);
    SIHD_UNEXPECTED_RETURN(added);
    return this->send(*answer);
}

template <BasicType T>
ObjectServer::Property ObjectServer::Property::read_only(const std::string & name, std::function<T()> getter)
{
    return Property {
        .name = name,
        .signature = std::string(1, dbus_type_of<T>()),
        .get = [getter = std::move(getter)](Appender & to) -> Result<void> { return to.add(getter()); },
        .set = {},
    };
}

template <BasicType T>
ObjectServer::Property ObjectServer::Property::bound(const std::string & name, T & value)
{
    return Property {
        .name = name,
        .signature = std::string(1, dbus_type_of<T>()),
        .get = [&value](Appender & to) -> Result<void> { return to.add(value); },
        .set = [&value](Iter & from) -> Result<void> {
            auto read = from.get<T>();
            SIHD_UNEXPECTED_RETURN(read);
            value = std::move(*read);
            return {};
        },
    };
}

# define SIHD_DBUS_INSTANTIATE(T)                                                                                      \
     template Result<T> ObjectServer::Call::argument<T>() const;                                                       \
     template Result<void> ObjectServer::Call::reply<T>(const T &);                                                    \
     template ObjectServer::Property ObjectServer::Property::read_only<T>(const std::string &, std::function<T()>);    \
     template ObjectServer::Property ObjectServer::Property::bound<T>(const std::string &, T &);
SIHD_DBUS_FOR_EACH_BASIC_TYPE(SIHD_DBUS_INSTANTIATE)
# undef SIHD_DBUS_INSTANTIATE

ObjectServer::ObjectServer(Connection & connection): _connection(connection) {}

Defer ObjectServer::_read_scope()
{
    return Defer([this, previous = std::exchange(_reading, true)] { _reading = previous; });
}

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

Result<void> ObjectServer::Call::fail(const Error & error)
{
    auto reply = _message.error_return(error_name_of(error.code), error.message);
    SIHD_UNEXPECTED_RETURN(reply);
    return this->send(*reply);
}

Result<void> ObjectServer::add_object(const std::string & path, std::vector<Interface> interfaces)
{
    if (_reading)
        return std::unexpected(getter_running());
    if (!dbus_validate_path(path.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus path: {}", path));
    if (_objects.contains(path))
        return std::unexpected(Error(already_exists, "dbus object {} already exists", path));
    auto valid = validate_interfaces(interfaces);
    SIHD_UNEXPECTED_RETURN(valid);

    _objects.emplace(path, std::move(interfaces));
    auto emitted = this->_emit_interfaces_added(path);
    if (!emitted.has_value())
        _objects.erase(path);
    return emitted;
}

Result<void> ObjectServer::remove_object(const std::string & path)
{
    if (_reading)
        return std::unexpected(getter_running());
    if (!_objects.contains(path))
        return std::unexpected(Error(not_found, "no dbus object at {}", path));
    auto emitted = this->_emit_interfaces_removed(path);
    SIHD_UNEXPECTED_RETURN(emitted);
    _objects.erase(path);
    return {};
}

Result<void> ObjectServer::add_object_manager(const std::string & path)
{
    if (!dbus_validate_path(path.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus path: {}", path));
    if (!_managers.emplace(path).second)
        return std::unexpected(Error(already_exists, "dbus object manager {} already exists", path));
    return {};
}

Result<void> ObjectServer::remove_object_manager(const std::string & path)
{
    if (_managers.erase(path) == 0)
        return std::unexpected(Error(not_found, "no dbus object manager at {}", path));
    return {};
}

Result<std::optional<Message>> ObjectServer::handle(Message && message)
{
    if (message.type() != Message::Type::method_call)
        return std::optional<Message>(std::move(message));
    Call call(Token {}, _connection, std::move(message));
    auto served = this->_serve(call);
    SIHD_UNEXPECTED_RETURN(served);
    return std::nullopt;
}

Result<Message> ObjectServer::call(const Message & request, Duration timeout)
{
    auto sent = request.send(_connection);
    SIHD_UNEXPECTED_RETURN(sent);
    const uint32_t serial = request.serial();

    auto answered = receive_matching(_connection, timeout, "call", [this, serial](Message & message) -> Result<bool> {
        const Message::Type type = message.type();
        const bool answer = type == Message::Type::method_return || type == Message::Type::error;
        if (answer && message.reply_serial() == serial)
            return true;
        auto handled = this->handle(std::move(message));
        SIHD_UNEXPECTED_RETURN(handled);
        return false;
    });
    SIHD_UNEXPECTED_RETURN(answered);
    if (answered->type() == Message::Type::error)
        return std::unexpected(answered->to_error());
    return answered;
}

Result<void> ObjectServer::emit_properties_changed(const std::string & path,
                                                   const std::string & interface,
                                                   const std::vector<std::string> & names)
{
    const Defer reading = this->_read_scope();
    const auto object = _objects.find(path);
    if (object == _objects.end())
        return std::unexpected(Error(not_found, "no dbus object at {}", path));
    const Interface *found_interface = find_interface(object->second, interface);
    if (found_interface == nullptr)
        return std::unexpected(Error(not_found, "no dbus interface {} on {}", interface, path));

    Message signal;
    auto built = signal.signal(path, properties_interface, "PropertiesChanged");
    SIHD_UNEXPECTED_RETURN(built);
    Appender body(signal);
    auto interface_added = body.add(interface);
    SIHD_UNEXPECTED_RETURN(interface_added);

    auto changed = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(changed);
    for (const std::string & name : names)
    {
        const Property *property = find_property(*found_interface, name);
        if (property == nullptr || !property->get)
            return std::unexpected(Error(not_found, "no readable dbus property {} on {}", name, interface));
        auto added = changed->add_entry(property->name, property->signature, property->get);
        SIHD_UNEXPECTED_RETURN(added);
    }
    auto changed_closed = body.close(std::move(*changed));
    SIHD_UNEXPECTED_RETURN(changed_closed);

    auto invalidated_added = body.add_array(std::vector<std::string> {});
    SIHD_UNEXPECTED_RETURN(invalidated_added);
    return signal.send(_connection);
}

Result<void> ObjectServer::_serve(Call & call)
{
    const std::string path = call.message().path();
    const std::string member = call.message().member();
    const auto object = _objects.find(path);
    const bool is_object = object != _objects.end();
    const bool is_manager = _managers.contains(path);

    // the interface of a call is optional: a method of the object wins over a standard one
    std::string interface = call.message().interface();
    if (interface.empty() && !(is_object && declares_method(object->second, member)))
        interface = standard_interface_of(member);

    if (interface == peer_interface)
        return serve_peer(call);

    if (!is_object && !is_manager && this->_children_of(path).empty())
        return answer_error(call, DBUS_ERROR_UNKNOWN_OBJECT, fmt::format("no object at {}", path));

    if (interface == introspectable_interface)
    {
        if (member != "Introspect")
            return answer_unknown_method(call);
        return has_arguments(call, "") ? call.reply(this->_introspect(path)) : answer_invalid_arguments(call, "");
    }
    if (is_manager && interface == object_manager_interface)
    {
        if (member != "GetManagedObjects")
            return answer_unknown_method(call);
        return has_arguments(call, "") ? this->_serve_managed_objects(call) : answer_invalid_arguments(call, "");
    }
    if (!is_object)
        return answer_error(call, DBUS_ERROR_UNKNOWN_OBJECT, fmt::format("no object at {}", path));
    if (interface == properties_interface)
        return this->_serve_properties(call, object->second);
    return this->_serve_method(call, object->second);
}

Result<void> ObjectServer::_serve_method(Call & call, const std::vector<Interface> & interfaces)
{
    const std::string interface = call.message().interface();
    const std::string member = call.message().member();

    if (!interface.empty() && find_interface(interfaces, interface) == nullptr)
        return answer_unknown_interface(call, interface);

    std::vector<const Method *> matches;
    for (const Interface & candidate : interfaces)
    {
        if (!interface.empty() && candidate.name != interface)
            continue;
        const auto found = std::ranges::find(candidate.methods, member, &Method::name);
        if (found != candidate.methods.end())
            matches.emplace_back(&*found);
    }
    if (matches.size() != 1)
        return answer_unknown_method(call);

    const std::string expected = joined_signature(matches[0]->in);
    if (!has_arguments(call, expected))
        return answer_invalid_arguments(call, expected);

    // copied because the handler can remove its own object
    const MethodHandler handler = matches[0]->handler;
    auto handled = handler(call);
    if (!handled.has_value() && call.pending())
        return call.fail(handled.error());
    SIHD_UNEXPECTED_RETURN(handled);
    return call.pending() ? call.reply() : Result<void>();
}

Result<void> ObjectServer::_serve_properties(Call & call, const std::vector<Interface> & interfaces)
{
    const std::string member = call.message().member();
    if (member == "Set")
        return has_arguments(call, "ssv") ? this->_serve_property_set(call, interfaces)
                                          : answer_invalid_arguments(call, "ssv");
    if (member != "Get" && member != "GetAll")
        return answer_unknown_method(call);
    const std::string_view expected = member == "Get" ? "ss" : "s";
    if (!has_arguments(call, expected))
        return answer_invalid_arguments(call, expected);

    const Defer reading = this->_read_scope();
    Iter arguments(call.message());
    auto interface_name = arguments.get<std::string>();
    SIHD_UNEXPECTED_RETURN(interface_name);
    const Interface *interface = find_interface(interfaces, *interface_name);
    if (interface == nullptr)
        return answer_unknown_interface(call, *interface_name);
    if (member == "GetAll")
        return answer_with(call, all_properties_reply(call, *interface));

    arguments.next();
    auto property_name = arguments.get<std::string>();
    SIHD_UNEXPECTED_RETURN(property_name);
    const Property *property = find_property(*interface, *property_name);
    if (property == nullptr)
        return answer_unknown_property(call, *property_name, *interface_name);
    if (!property->get)
        return answer_error(call, DBUS_ERROR_INVALID_ARGS, fmt::format("property {} is write only", *property_name));
    return answer_with(call, property_get_reply(call, *property));
}

Result<void> ObjectServer::_serve_property_set(Call & call, const std::vector<Interface> & interfaces)
{
    const std::string path = call.message().path();
    Iter arguments(call.message());
    auto interface_name = arguments.get<std::string>();
    SIHD_UNEXPECTED_RETURN(interface_name);
    arguments.next();
    auto property_name = arguments.get<std::string>();
    SIHD_UNEXPECTED_RETURN(property_name);
    arguments.next();

    const Interface *interface = find_interface(interfaces, *interface_name);
    if (interface == nullptr)
        return answer_unknown_interface(call, *interface_name);
    const Property *property = find_property(*interface, *property_name);
    if (property == nullptr)
        return answer_unknown_property(call, *property_name, *interface_name);
    if (!property->set)
    {
        return answer_error(call,
                            DBUS_ERROR_PROPERTY_READ_ONLY,
                            fmt::format("property {} is read only", *property_name));
    }
    auto value = arguments.recurse();
    SIHD_UNEXPECTED_RETURN(value);
    if (value->signature() != property->signature)
    {
        return answer_error(
            call,
            DBUS_ERROR_INVALID_ARGS,
            fmt::format("property {} is '{}', got '{}'", *property_name, property->signature, value->signature()));
    }

    // copied because the setter can remove its own object
    const auto setter = property->set;
    const bool emits_changed = property->emits_changed && property->get;
    auto stored = setter(*value);
    if (!stored.has_value())
        return call.fail(stored.error());
    auto answered = call.reply();
    SIHD_UNEXPECTED_RETURN(answered);
    if (!emits_changed || !_objects.contains(path))
        return {};
    return this->emit_properties_changed(path, *interface_name, {*property_name});
}

Result<void> ObjectServer::_serve_managed_objects(Call & call)
{
    const Defer reading = this->_read_scope();
    return answer_with(call, managed_objects_reply(call, _objects));
}

Result<void> ObjectServer::_emit_interfaces_added(const std::string & path)
{
    const Defer reading = this->_read_scope();
    for (const std::string & manager : this->_managers_of(path))
    {
        Message signal;
        auto built = signal.signal(manager, object_manager_interface, "InterfacesAdded");
        SIHD_UNEXPECTED_RETURN(built);
        Appender body(signal);
        auto path_added = body.add_object_path(path);
        SIHD_UNEXPECTED_RETURN(path_added);
        auto interfaces_written = write_interfaces(body, _objects.at(path));
        SIHD_UNEXPECTED_RETURN(interfaces_written);
        auto sent = signal.send(_connection);
        SIHD_UNEXPECTED_RETURN(sent);
    }
    return {};
}

Result<void> ObjectServer::_emit_interfaces_removed(const std::string & path)
{
    std::vector<std::string> names;
    for (const Interface & interface : _objects.at(path))
        names.emplace_back(interface.name);
    for (const std::string & manager : this->_managers_of(path))
    {
        Message signal;
        auto built = signal.signal(manager, object_manager_interface, "InterfacesRemoved");
        SIHD_UNEXPECTED_RETURN(built);
        Appender body(signal);
        auto path_added = body.add_object_path(path);
        SIHD_UNEXPECTED_RETURN(path_added);
        auto names_added = body.add_array(names);
        SIHD_UNEXPECTED_RETURN(names_added);
        auto sent = signal.send(_connection);
        SIHD_UNEXPECTED_RETURN(sent);
    }
    return {};
}

std::vector<std::string> ObjectServer::_managers_of(const std::string & path) const
{
    std::vector<std::string> managers;
    for (const std::string & manager : _managers)
    {
        if (manager != path && path.starts_with(child_prefix(manager)))
            managers.emplace_back(manager);
    }
    return managers;
}

std::vector<std::string> ObjectServer::_children_of(const std::string & path) const
{
    std::set<std::string> children;
    for (const auto & [object_path, interfaces] : _objects)
        add_child(children, path, object_path);
    for (const std::string & manager : _managers)
        add_child(children, path, manager);
    return {children.begin(), children.end()};
}

std::string ObjectServer::_introspect(const std::string & path) const
{
    std::string xml = DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE "<node>\n";
    xml += peer_xml;
    xml += introspectable_xml;
    const auto object = _objects.find(path);
    if (object != _objects.end())
    {
        xml += properties_xml;
        for (const Interface & interface : object->second)
            xml += interface_xml(interface);
    }
    if (_managers.contains(path))
        xml += object_manager_xml;
    for (const std::string & child : this->_children_of(path))
        xml += fmt::format("  <node name=\"{}\"/>\n", child);
    xml += "</node>\n";
    return xml;
}

# else

Result<void> ObjectServer::Call::fail(const Error &)
{
    return unsupported_platform();
}

Result<void> ObjectServer::add_object(const std::string &, std::vector<Interface>)
{
    return unsupported_platform();
}

Result<void> ObjectServer::remove_object(const std::string &)
{
    return unsupported_platform();
}

Result<void> ObjectServer::add_object_manager(const std::string &)
{
    return unsupported_platform();
}

Result<void> ObjectServer::remove_object_manager(const std::string &)
{
    return unsupported_platform();
}

Result<std::optional<Message>> ObjectServer::handle(Message &&)
{
    return unsupported_platform();
}

Result<Message> ObjectServer::call(const Message &, Duration)
{
    return unsupported_platform();
}

Result<void>
    ObjectServer::emit_properties_changed(const std::string &, const std::string &, const std::vector<std::string> &)
{
    return unsupported_platform();
}

# endif

} // namespace sihd::sys::linux_::dbus

#endif
