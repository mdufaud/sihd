#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <optional>

# include <sihd/util/Clocks.hpp>

# include "Impl.hpp"

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

void release_pending(::DBusPendingCall *pending)
{
    dbus_pending_call_cancel(pending);
    dbus_pending_call_unref(pending);
}

using PendingCall = std::unique_ptr<::DBusPendingCall, decltype(&release_pending)>;

// a sent message is locked: libdbus refuses to change its flags
void forbid_auto_start(::DBusMessage *message)
{
    if (dbus_message_get_serial(message) == 0 && dbus_message_get_auto_start(message))
        dbus_message_set_auto_start(message, FALSE);
}

// libdbus silently drops a message holding fds on a connection that cannot pass them
Result<void> check_fds_sendable(::DBusConnection *connection, ::DBusMessage *message)
{
    if (dbus_message_contains_unix_fds(message) && !dbus_connection_can_send_type(connection, DBUS_TYPE_UNIX_FD))
        return std::unexpected(Error(not_supported, "this dbus connection cannot pass unix fds"));
    return {};
}

Result<void> prepare_outgoing(::DBusConnection *connection, ::DBusMessage *message, bool auto_start)
{
    auto sendable = check_fds_sendable(connection, message);
    SIHD_UNEXPECTED_RETURN(sendable);
    if (!auto_start)
        forbid_auto_start(message);
    return {};
}

Result<void> validate_names(ZStringView path, ZStringView interface, ZStringView member)
{
    if (!dbus_validate_path(path.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus path: {}", path));
    if (!interface.empty() && !dbus_validate_interface(interface.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus interface: {}", interface));
    if (!dbus_validate_member(member.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus member: {}", member));
    return {};
}

Result<void> check_answerable(::DBusMessage *call)
{
    if (call == nullptr)
        return std::unexpected(empty_message());
    if (dbus_message_get_type(call) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return std::unexpected(Error(invalid_argument, "a dbus reply answers a method call"));
    if (dbus_message_get_serial(call) == 0)
        return std::unexpected(Error(invalid_argument, "a dbus reply answers a received method call"));
    return {};
}

template <typename ImplPointer>
std::string field_of(const ImplPointer & impl, const char *(*getter)(::DBusMessage *))
{
    return impl != nullptr ? string_or_empty(getter(impl->message)) : std::string();
}

} // namespace

Message::Impl::Impl(::DBusMessage *adopted): message(adopted) {}

Message::Impl::~Impl()
{
    dbus_message_unref(message);
}

Message Message::Impl::wrap(::DBusMessage *message)
{
    return Message(std::make_unique<Impl>(message));
}

Result<Message> Message::Impl::created(::DBusMessage *message, std::string_view what)
{
    if (message == nullptr)
        return std::unexpected(Error(out_of_memory, "could not create dbus {}", what));
    return wrap(message);
}

Result<void> Message::method_call(ZStringView destination, ZStringView path, ZStringView interface, ZStringView member)
{
    if (!destination.empty() && !dbus_validate_bus_name(destination.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus destination: {}", destination));
    auto valid = validate_names(path, interface, member);
    SIHD_UNEXPECTED_RETURN(valid);

    auto created = Impl::created(dbus_message_new_method_call(destination.empty() ? nullptr : destination.c_str(),
                                                              path.c_str(),
                                                              interface.empty() ? nullptr : interface.c_str(),
                                                              member.c_str()),
                                 "method call");
    SIHD_UNEXPECTED_RETURN(created);
    *this = std::move(*created);
    return {};
}

Result<void> Message::signal(ZStringView path, ZStringView interface, ZStringView member)
{
    if (interface.empty())
        return std::unexpected(Error(invalid_argument, "a dbus signal needs an interface"));
    auto valid = validate_names(path, interface, member);
    SIHD_UNEXPECTED_RETURN(valid);

    auto created = Impl::created(dbus_message_new_signal(path.c_str(), interface.c_str(), member.c_str()), "signal");
    SIHD_UNEXPECTED_RETURN(created);
    *this = std::move(*created);
    return {};
}

Result<Message> Message::method_return() const
{
    auto answerable = check_answerable(_impl != nullptr ? _impl->message : nullptr);
    SIHD_UNEXPECTED_RETURN(answerable);

    return Impl::created(dbus_message_new_method_return(_impl->message), "method return");
}

Result<Message> Message::error_return(ZStringView name, ZStringView text) const
{
    auto answerable = check_answerable(_impl != nullptr ? _impl->message : nullptr);
    SIHD_UNEXPECTED_RETURN(answerable);
    if (!dbus_validate_error_name(name.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus error name: {}", name));
    if (!dbus_validate_utf8(text.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "dbus string is not valid utf-8"));

    return Impl::created(dbus_message_new_error(_impl->message, name.c_str(), text.c_str()), "error return");
}

std::string Message::path() const
{
    return field_of(_impl, dbus_message_get_path);
}

std::string Message::interface() const
{
    return field_of(_impl, dbus_message_get_interface);
}

std::string Message::member() const
{
    return field_of(_impl, dbus_message_get_member);
}

std::string Message::sender() const
{
    return field_of(_impl, dbus_message_get_sender);
}

std::string Message::destination() const
{
    return field_of(_impl, dbus_message_get_destination);
}

std::string Message::error_name() const
{
    return field_of(_impl, dbus_message_get_error_name);
}

std::string Message::signature() const
{
    return field_of(_impl, dbus_message_get_signature);
}

Message::Type Message::type() const
{
    if (_impl == nullptr)
        return Type::invalid;
    switch (dbus_message_get_type(_impl->message))
    {
        case DBUS_MESSAGE_TYPE_METHOD_CALL:
            return Type::method_call;
        case DBUS_MESSAGE_TYPE_METHOD_RETURN:
            return Type::method_return;
        case DBUS_MESSAGE_TYPE_ERROR:
            return Type::error;
        case DBUS_MESSAGE_TYPE_SIGNAL:
            return Type::signal;
        default:
            return Type::invalid;
    }
}

uint32_t Message::serial() const
{
    return _impl != nullptr ? dbus_message_get_serial(_impl->message) : 0;
}

uint32_t Message::reply_serial() const
{
    return _impl != nullptr ? dbus_message_get_reply_serial(_impl->message) : 0;
}

bool Message::expects_reply() const
{
    return this->type() == Type::method_call && !dbus_message_get_no_reply(_impl->message);
}

Result<void> Message::set_no_reply()
{
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    // never change a sent or received message: it is locked and libdbus aborts
    if (dbus_message_get_serial(_impl->message) != 0)
        return std::unexpected(Error(invalid_argument, "dbus message was already sent or received"));
    dbus_message_set_no_reply(_impl->message, TRUE);
    return {};
}

Error Message::to_error() const
{
    if (_impl == nullptr)
        return empty_message();
    if (this->type() != Type::error)
        return Error(invalid_argument, "dbus message is no error reply");
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    dbus_set_error_from_message(&dbus_error, _impl->message);
    return dbus_error_take(dbus_error);
}

Result<Message> Message::call(Connection & connection, Duration timeout) const
{
    if (connection._impl == nullptr)
        return std::unexpected(connection_not_open());
    if (_impl == nullptr)
        return std::unexpected(empty_message());

    auto prepared = prepare_outgoing(connection._impl->connection, _impl->message, connection._auto_start);
    SIHD_UNEXPECTED_RETURN(prepared);
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    ::DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection._impl->connection,
                                                                     _impl->message,
                                                                     timeout.milliseconds_int(),
                                                                     &dbus_error);
    if (reply == nullptr)
        return std::unexpected(dbus_error_take(dbus_error));
    return Impl::wrap(reply);
}

Result<std::vector<Message>>
    Message::call_all(Connection & connection, const std::vector<Message> & requests, Duration timeout)
{
    if (connection._impl == nullptr)
        return std::unexpected(connection_not_open());

    std::vector<PendingCall> pending_calls;
    pending_calls.reserve(requests.size());
    for (const Message & request : requests)
    {
        if (request._impl == nullptr)
            return std::unexpected(empty_message());
        auto prepared = prepare_outgoing(connection._impl->connection, request._impl->message, connection._auto_start);
        SIHD_UNEXPECTED_RETURN(prepared);
        ::DBusPendingCall *pending = nullptr;
        if (!dbus_connection_send_with_reply(connection._impl->connection,
                                             request._impl->message,
                                             &pending,
                                             timeout.milliseconds_int()))
            return std::unexpected(Error(out_of_memory, "could not send dbus message"));
        if (pending == nullptr)
            return std::unexpected(Error(closed, "dbus connection lost"));
        pending_calls.emplace_back(pending, &release_pending);
    }
    dbus_connection_flush(connection._impl->connection);

    std::vector<Message> replies;
    replies.reserve(requests.size());
    for (const PendingCall & pending : pending_calls)
    {
        dbus_pending_call_block(pending.get());
        ::DBusMessage *stolen = dbus_pending_call_steal_reply(pending.get());
        if (stolen == nullptr)
            return std::unexpected(Error(io_error, "dbus call completed without a reply"));
        Message reply = Impl::wrap(stolen);
        if (reply.type() == Type::error)
            return std::unexpected(reply.to_error());
        replies.emplace_back(std::move(reply));
    }
    return replies;
}

Result<void> Message::send(Connection & connection) const
{
    if (connection._impl == nullptr)
        return std::unexpected(connection_not_open());
    if (_impl == nullptr)
        return std::unexpected(empty_message());
    auto prepared = prepare_outgoing(connection._impl->connection, _impl->message, connection._auto_start);
    SIHD_UNEXPECTED_RETURN(prepared);
    if (!dbus_connection_send(connection._impl->connection, _impl->message, nullptr))
        return std::unexpected(Error(out_of_memory, "could not send dbus message"));
    dbus_connection_flush(connection._impl->connection);
    if (dbus_connection_get_dispatch_status(connection._impl->connection) == DBUS_DISPATCH_DATA_REMAINS)
        return connection._impl->queued.notify();
    return {};
}

Result<bool> Message::receive(Connection & connection, Duration timeout)
{
    Connection::Impl *impl = connection._impl.get();
    if (impl == nullptr)
        return std::unexpected(connection_not_open());

    SteadyClock clock;
    const Timestamp deadline = clock.now() + timeout;
    while (true)
    {
        dbus_connection_read_write(impl->connection, 0);
        if (::DBusMessage *message = dbus_connection_pop_message(impl->connection))
        {
            *this = Impl::wrap(message);
            return true;
        }
        if (!dbus_connection_get_is_connected(impl->connection))
            return std::unexpected(Error(closed, "dbus connection lost"));

        const Timestamp now = clock.now();
        if (now >= deadline)
            return false;
        if (impl->poll.poll(Duration(deadline - now).milliseconds_int()) < 0)
            return std::unexpected(Error(io_error, "could not poll the dbus connection"));
        auto queued = impl->queued.read();
        SIHD_UNEXPECTED_RETURN(queued);
        auto woken = impl->wake.read();
        SIHD_UNEXPECTED_RETURN(woken);
        if (*woken > 0)
            return false;
    }
}

# else

Result<void> Message::method_call(ZStringView, ZStringView, ZStringView, ZStringView)
{
    return unsupported_platform();
}

Result<void> Message::signal(ZStringView, ZStringView, ZStringView)
{
    return unsupported_platform();
}

Result<Message> Message::method_return() const
{
    return unsupported_platform();
}

Result<Message> Message::error_return(ZStringView, ZStringView) const
{
    return unsupported_platform();
}

std::string Message::path() const
{
    return {};
}

std::string Message::interface() const
{
    return {};
}

std::string Message::member() const
{
    return {};
}

std::string Message::sender() const
{
    return {};
}

std::string Message::destination() const
{
    return {};
}

std::string Message::error_name() const
{
    return {};
}

std::string Message::signature() const
{
    return {};
}

Message::Type Message::type() const
{
    return Type::invalid;
}

uint32_t Message::serial() const
{
    return 0;
}

uint32_t Message::reply_serial() const
{
    return 0;
}

bool Message::expects_reply() const
{
    return false;
}

Result<void> Message::set_no_reply()
{
    return unsupported_platform();
}

Error Message::to_error() const
{
    return unsupported_platform().error();
}

Result<Message> Message::call(Connection &, Duration) const
{
    return unsupported_platform();
}

Result<std::vector<Message>> Message::call_all(Connection &, const std::vector<Message> &, Duration)
{
    return unsupported_platform();
}

Result<void> Message::send(Connection &) const
{
    return unsupported_platform();
}

Result<bool> Message::receive(Connection &, Duration)
{
    return unsupported_platform();
}

# endif

Message::Message() = default;

Message::Message(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Message::Message(Message && other) noexcept = default;

Message & Message::operator=(Message && other) noexcept = default;

Message::~Message() = default;

Result<void> Message::add(ZStringView value)
{
    return Appender(*this).add(value);
}

Result<void> Message::add_object_path(ZStringView path)
{
    return Appender(*this).add_object_path(path);
}

Result<void> Message::add_signature(ZStringView signature)
{
    return Appender(*this).add_signature(signature);
}

Result<void> Message::add_unix_fd(int fd)
{
    return Appender(*this).add_unix_fd(fd);
}

template <FixedType T>
Result<void> Message::add(const T & value)
{
    return Appender(*this).add(value);
}

Result<void> Message::properties_get(ZStringView destination, ZStringView path, ZStringView interface, ZStringView name)
{
    Message request;
    auto built = request.method_call(destination, path, properties_interface, "Get");
    SIHD_UNEXPECTED_RETURN(built);
    auto interface_added = request.add(interface);
    SIHD_UNEXPECTED_RETURN(interface_added);
    auto name_added = request.add(name);
    SIHD_UNEXPECTED_RETURN(name_added);
    *this = std::move(request);
    return {};
}

Result<void> Message::properties_get_all(ZStringView destination, ZStringView path, ZStringView interface)
{
    Message request;
    auto built = request.method_call(destination, path, properties_interface, "GetAll");
    SIHD_UNEXPECTED_RETURN(built);
    auto interface_added = request.add(interface);
    SIHD_UNEXPECTED_RETURN(interface_added);
    *this = std::move(request);
    return {};
}

Result<std::optional<Iter>> Message::changed_property(std::string_view interface, std::string_view name) const
{
    if (this->type() == Type::invalid)
        return std::unexpected(empty_message());
    if (this->type() != Type::signal || this->interface() != properties_interface
        || this->member() != "PropertiesChanged")
        return std::nullopt;
    Iter body(*this);
    auto changed_interface = body.get<std::string>();
    SIHD_UNEXPECTED_RETURN(changed_interface);
    if (*changed_interface != interface)
        return std::nullopt;
    body.next();

    std::optional<Iter> changed;
    auto walked = body.for_each_entry<std::string>(
        [&changed, &name](const std::string & key, Iter & value) -> Result<void> {
            if (key == name)
                changed.emplace(std::move(value));
            return {};
        });
    SIHD_UNEXPECTED_RETURN(walked);
    return changed;
}

Result<Message> receive_matching(Connection & connection,
                                 Duration timeout,
                                 std::string_view what,
                                 const std::function<Result<bool>(Message &)> & matches)
{
    SteadyClock clock;
    const Timestamp deadline = clock.now() + timeout;
    while (true)
    {
        const Timestamp now = clock.now();
        if (now >= deadline)
            return std::unexpected(Error(ErrorCode::timeout, "dbus {} timed out", what));
        Message incoming;
        auto received = incoming.receive(connection, Duration(deadline - now));
        SIHD_UNEXPECTED_RETURN(received);
        if (!*received)
        {
            if (clock.now() >= deadline)
                continue;
            return std::unexpected(Error(interrupted, "dbus {} interrupted", what));
        }
        auto matched = matches(incoming);
        SIHD_UNEXPECTED_RETURN(matched);
        if (*matched)
            return incoming;
    }
}

Result<void> Message::receive_until(Connection & connection,
                                    Duration timeout,
                                    const std::function<Result<bool>(const Message &)> & accepted)
{
    auto received = receive_matching(connection, timeout, "receive", accepted);
    SIHD_UNEXPECTED_RETURN(received);
    *this = std::move(*received);
    return {};
}

# define SIHD_DBUS_INSTANTIATE(T) template Result<void> Message::add<T>(const T &);
SIHD_DBUS_FOR_EACH_FIXED_TYPE(SIHD_DBUS_INSTANTIATE)
# undef SIHD_DBUS_INSTANTIATE

} // namespace sihd::sys::linux_::dbus

#endif
