#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <string>
# include <string_view>
# include <type_traits>
# include <utility>

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
#  include <dbus/dbus.h>

#  include <sihd/sys/Poll.hpp>
#  include <sihd/sys/linux/EventFd.hpp>
# endif

# include <fmt/format.h>

# include <sihd/sys/linux/dbus.hpp>
# include <sihd/util/Clocks.hpp>

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

ErrorCode code_of(const ::DBusError & dbus_error)
{
    if (dbus_error_has_name(&dbus_error, DBUS_ERROR_ACCESS_DENIED))
        return permission_denied;
    if (dbus_error_has_name(&dbus_error, DBUS_ERROR_NO_REPLY))
        return ErrorCode::timeout;
    return io_error;
}

Error dbus_error_take(::DBusError & dbus_error)
{
    Error error(code_of(dbus_error), "{}: {}", dbus_error.name, dbus_error.message);
    dbus_error_free(&dbus_error);
    return error;
}

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

template <typename T>
constexpr int dbus_type_of()
{
    if constexpr (std::is_same_v<T, bool>)
        return DBUS_TYPE_BOOLEAN;
    else if constexpr (std::is_same_v<T, uint8_t>)
        return DBUS_TYPE_BYTE;
    else if constexpr (std::is_same_v<T, int16_t>)
        return DBUS_TYPE_INT16;
    else if constexpr (std::is_same_v<T, uint16_t>)
        return DBUS_TYPE_UINT16;
    else if constexpr (std::is_same_v<T, int32_t>)
        return DBUS_TYPE_INT32;
    else
        return DBUS_TYPE_UINT32;
}

bool is_string_type(int type)
{
    return type == DBUS_TYPE_STRING || type == DBUS_TYPE_OBJECT_PATH || type == DBUS_TYPE_SIGNATURE;
}

bool is_container_type(int type)
{
    return type == DBUS_TYPE_ARRAY || type == DBUS_TYPE_STRUCT || type == DBUS_TYPE_VARIANT
           || type == DBUS_TYPE_DICT_ENTRY;
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

std::string string_or_empty(const char *str)
{
    return str != nullptr ? std::string(str) : std::string();
}

Error type_mismatch(int expected, int actual)
{
    return Error(invalid_argument,
                 "dbus type mismatch: expected '{}', got '{}'",
                 static_cast<char>(expected),
                 static_cast<char>(actual));
}

Error signature_mismatch(const std::string & expected, const std::string & actual)
{
    return Error(invalid_argument, "dbus type mismatch: expected '{}', got '{}'", expected, actual);
}

Result<void> check_answerable(::DBusMessage *call)
{
    if (call == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if (dbus_message_get_type(call) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return std::unexpected(Error(invalid_argument, "a dbus reply answers a method call"));
    if (dbus_message_get_serial(call) == 0)
        return std::unexpected(Error(invalid_argument, "a dbus reply answers a received method call"));
    return {};
}

std::string first_complete_type(const std::string & signature)
{
    ::DBusSignatureIter iter;
    dbus_signature_iter_init(&iter, signature.c_str());
    char *first = dbus_signature_iter_get_signature(&iter);
    std::string ret(first);
    dbus_free(first);
    return ret;
}

} // namespace

struct Connection::Impl
{
        ::DBusConnection *connection = nullptr;
        // receive() waits on the connection socket and on this wake up counter
        EventFd wake;
        Poll poll {2};

        ~Impl()
        {
            dbus_connection_close(connection);
            dbus_connection_unref(connection);
        }

        static Result<std::unique_ptr<Impl>> adopt(::DBusConnection *connection)
        {
            // libdbus calls _exit() on a lost bus otherwise
            dbus_connection_set_exit_on_disconnect(connection, FALSE);
            auto impl = std::make_unique<Impl>();
            impl->connection = connection;

            int fd = -1;
            if (!dbus_connection_get_unix_fd(connection, &fd))
                return std::unexpected(Error(io_error, "dbus connection has no unix socket"));
            auto wake_opened = impl->wake.open();
            SIHD_UNEXPECTED_RETURN(wake_opened);
            impl->poll.set_read_fd(fd);
            impl->poll.set_read_fd(impl->wake.fd());
            return impl;
        }
};

struct Message::Impl
{
        ::DBusMessage *message = nullptr;

        ~Impl() { dbus_message_unref(message); }
};

struct Iter::Impl
{
        ::DBusMessageIter iter {};
};

Iter::Iter(const Message & message)
{
    if (message._impl != nullptr)
    {
        _impl = std::make_unique<Impl>();
        dbus_message_iter_init(message._impl->message, &_impl->iter);
    }
}

char Iter::type() const
{
    if (_impl == nullptr)
        return '\0';
    return static_cast<char>(dbus_message_iter_get_arg_type(&_impl->iter));
}

bool Iter::next()
{
    if (_impl == nullptr)
        return false;
    return dbus_message_iter_next(&_impl->iter);
}

template <BasicType T>
Result<T> Iter::get() const
{
    const int actual = this->type();
    if constexpr (std::is_same_v<T, std::string>)
    {
        if (!is_string_type(actual))
            return std::unexpected(type_mismatch(DBUS_TYPE_STRING, actual));
        const char *value = nullptr;
        dbus_message_iter_get_basic(&_impl->iter, &value);
        return std::string(value);
    }
    else
    {
        constexpr int expected = dbus_type_of<T>();
        if (actual != expected)
            return std::unexpected(type_mismatch(expected, actual));

        if constexpr (std::is_same_v<T, bool>)
        {
            dbus_bool_t value = 0;
            dbus_message_iter_get_basic(&_impl->iter, &value);
            return value != 0;
        }
        else
        {
            T value {};
            dbus_message_iter_get_basic(&_impl->iter, &value);
            return value;
        }
    }
}

Result<Iter> Iter::recurse() const
{
    const int actual = this->type();
    if (!is_container_type(actual))
        return std::unexpected(Error(invalid_argument, "dbus type '{}' is not a container", static_cast<char>(actual)));
    auto sub = std::make_unique<Impl>();
    dbus_message_iter_recurse(&_impl->iter, &sub->iter);
    return Iter(std::move(sub));
}

// libdbus aborts on what its writer does not expect: types and parent/child order are checked before each write
struct Appender::Impl
{
        ::DBusMessageIter iter {};
        Impl *parent = nullptr;
        Impl *child = nullptr;
        // a dropped open container leaves the whole message unusable
        bool abandoned = false;
        bool constrained = false;
        bool repeat = false;
        std::string expected;

        ~Impl()
        {
            if (parent != nullptr)
                this->abandon();
            else if (child != nullptr)
                child->abandon();
        }

        void abandon()
        {
            if (child != nullptr)
                child->abandon();
            dbus_message_iter_abandon_container_if_open(&parent->iter, &iter);
            for (Impl *ancestor = parent; ancestor != nullptr; ancestor = ancestor->parent)
                ancestor->abandoned = true;
            parent->child = nullptr;
            parent = nullptr;
        }

        Result<void> check_writable() const
        {
            if (abandoned)
                return std::unexpected(Error(invalid_argument, "dbus message has an abandoned container"));
            if (child != nullptr)
                return std::unexpected(Error(invalid_argument, "dbus container has an open child"));
            return {};
        }

        Result<std::string> next_type() const
        {
            auto writable = this->check_writable();
            SIHD_UNEXPECTED_RETURN(writable);
            if (!constrained)
                return std::string();
            if (expected.empty())
                return std::unexpected(Error(invalid_argument, "dbus container takes no more values"));
            return first_complete_type(expected);
        }

        Result<std::string> expect(const std::string & type) const
        {
            auto next = this->next_type();
            SIHD_UNEXPECTED_RETURN(next);
            if (!next->empty() && *next != type)
                return std::unexpected(signature_mismatch(*next, type));
            return next;
        }

        Result<std::string> expect_opening(char opening) const
        {
            auto next = this->next_type();
            SIHD_UNEXPECTED_RETURN(next);
            if (!next->empty() && !next->starts_with(opening))
                return std::unexpected(signature_mismatch(*next, std::string(1, opening)));
            return next;
        }

        void consume(const std::string & type)
        {
            if (!repeat)
                expected.erase(0, type.size());
        }

        Result<void> append(int type, const void *value)
        {
            auto next = this->expect(std::string(1, static_cast<char>(type)));
            SIHD_UNEXPECTED_RETURN(next);
            if (!dbus_message_iter_append_basic(&iter, type, value))
                return std::unexpected(Error(out_of_memory, "could not append dbus argument"));
            this->consume(*next);
            return {};
        }

        Result<std::unique_ptr<Impl>>
            open_child(int type, const char *signature, const std::string & consumed, std::unique_ptr<Impl> container)
        {
            if (!dbus_message_iter_open_container(&iter, type, signature, &container->iter))
                return std::unexpected(Error(out_of_memory, "could not open dbus container"));
            this->consume(consumed);
            container->parent = this;
            child = container.get();
            return container;
        }

        static std::unique_ptr<Impl> constrained_to(const std::string & expected, bool repeat)
        {
            auto container = std::make_unique<Impl>();
            container->constrained = true;
            container->repeat = repeat;
            container->expected = expected;
            return container;
        }
};

Appender::Appender(Message & message)
{
    if (message._impl != nullptr)
    {
        _impl = std::make_unique<Impl>();
        dbus_message_iter_init_append(message._impl->message, &_impl->iter);
    }
}

Result<void> Appender::add(ZStringView value)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if (!dbus_validate_utf8(value.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "dbus string is not valid utf-8"));
    const char *str = value.c_str();
    return _impl->append(DBUS_TYPE_STRING, &str);
}

Result<void> Appender::add_object_path(ZStringView path)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if (!dbus_validate_path(path.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus path: {}", path));
    const char *str = path.c_str();
    return _impl->append(DBUS_TYPE_OBJECT_PATH, &str);
}

template <BasicType T>
Result<void> Appender::add_array(const std::vector<T> & values)
{
    char element = DBUS_TYPE_STRING;
    if constexpr (!std::is_same_v<T, std::string>)
        element = static_cast<char>(dbus_type_of<T>());
    auto array = this->open_array(std::string(1, element));
    SIHD_UNEXPECTED_RETURN(array);
    for (const T & value : values)
    {
        auto added = array->add(value);
        SIHD_UNEXPECTED_RETURN(added);
    }
    return this->close(std::move(*array));
}

template <FixedType T>
Result<void> Appender::add(const T & value)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if constexpr (std::is_same_v<T, bool>)
    {
        const dbus_bool_t dbus_value = value ? TRUE : FALSE;
        return _impl->append(DBUS_TYPE_BOOLEAN, &dbus_value);
    }
    else
        return _impl->append(dbus_type_of<T>(), &value);
}

Result<Appender> Appender::open_array(ZStringView element_signature)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    const std::string type = fmt::format("a{}", element_signature);
    if (!dbus_signature_validate_single(type.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus array element signature: {}", element_signature));
    auto next = _impl->expect(type);
    SIHD_UNEXPECTED_RETURN(next);

    auto opened = _impl->open_child(DBUS_TYPE_ARRAY,
                                    element_signature.c_str(),
                                    *next,
                                    Impl::constrained_to(std::string(element_signature.view()), true));
    SIHD_UNEXPECTED_RETURN(opened);
    return Appender(std::move(*opened));
}

Result<Appender> Appender::open_variant(ZStringView value_signature)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if (!dbus_signature_validate_single(value_signature.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus variant signature: {}", value_signature));
    auto next = _impl->expect("v");
    SIHD_UNEXPECTED_RETURN(next);

    auto opened = _impl->open_child(DBUS_TYPE_VARIANT,
                                    value_signature.c_str(),
                                    *next,
                                    Impl::constrained_to(std::string(value_signature.view()), false));
    SIHD_UNEXPECTED_RETURN(opened);
    return Appender(std::move(*opened));
}

Result<Appender> Appender::open_struct()
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    auto next = _impl->expect_opening('(');
    SIHD_UNEXPECTED_RETURN(next);

    auto container = next->empty() ? std::make_unique<Impl>()
                                   : Impl::constrained_to(next->substr(1, next->size() - 2), false);
    auto opened = _impl->open_child(DBUS_TYPE_STRUCT, nullptr, *next, std::move(container));
    SIHD_UNEXPECTED_RETURN(opened);
    return Appender(std::move(*opened));
}

Result<Appender> Appender::open_dict_entry()
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    auto next = _impl->expect_opening('{');
    SIHD_UNEXPECTED_RETURN(next);
    if (next->empty())
        return std::unexpected(Error(invalid_argument, "a dbus dict entry only goes in an array of dict entries"));

    auto opened = _impl->open_child(DBUS_TYPE_DICT_ENTRY,
                                    nullptr,
                                    *next,
                                    Impl::constrained_to(next->substr(1, next->size() - 2), false));
    SIHD_UNEXPECTED_RETURN(opened);
    return Appender(std::move(*opened));
}

Result<void> Appender::close(Appender && container)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if (container._impl == nullptr || container._impl->parent != _impl.get())
        return std::unexpected(Error(invalid_argument, "dbus container was not opened by this appender"));
    auto writable = container._impl->check_writable();
    SIHD_UNEXPECTED_RETURN(writable);
    if (!container._impl->repeat && !container._impl->expected.empty())
        return std::unexpected(Error(invalid_argument, "dbus container misses '{}'", container._impl->expected));

    std::unique_ptr<Impl> closed = std::move(container._impl);
    closed->parent = nullptr;
    _impl->child = nullptr;
    if (!dbus_message_iter_close_container(&_impl->iter, &closed->iter))
        return std::unexpected(Error(out_of_memory, "could not close dbus container"));
    return {};
}

Result<Message>
    Message::method_call(ZStringView destination, ZStringView path, ZStringView interface, ZStringView member)
{
    if (!destination.empty() && !dbus_validate_bus_name(destination.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus destination: {}", destination));
    auto valid = validate_names(path, interface, member);
    SIHD_UNEXPECTED_RETURN(valid);

    ::DBusMessage *message = dbus_message_new_method_call(destination.empty() ? nullptr : destination.c_str(),
                                                          path.c_str(),
                                                          interface.empty() ? nullptr : interface.c_str(),
                                                          member.c_str());
    if (message == nullptr)
        return std::unexpected(Error(out_of_memory, "could not create dbus method call"));
    auto impl = std::make_unique<Impl>();
    impl->message = message;
    return Message(std::move(impl));
}

Result<Message> Message::signal(ZStringView path, ZStringView interface, ZStringView member)
{
    if (interface.empty())
        return std::unexpected(Error(invalid_argument, "a dbus signal needs an interface"));
    auto valid = validate_names(path, interface, member);
    SIHD_UNEXPECTED_RETURN(valid);

    ::DBusMessage *message = dbus_message_new_signal(path.c_str(), interface.c_str(), member.c_str());
    if (message == nullptr)
        return std::unexpected(Error(out_of_memory, "could not create dbus signal"));
    auto impl = std::make_unique<Impl>();
    impl->message = message;
    return Message(std::move(impl));
}

Result<Message> Message::method_return() const
{
    auto answerable = check_answerable(_impl != nullptr ? _impl->message : nullptr);
    SIHD_UNEXPECTED_RETURN(answerable);

    ::DBusMessage *reply = dbus_message_new_method_return(_impl->message);
    if (reply == nullptr)
        return std::unexpected(Error(out_of_memory, "could not create dbus method return"));
    auto impl = std::make_unique<Impl>();
    impl->message = reply;
    return Message(std::move(impl));
}

Result<Message> Message::error_return(ZStringView name, ZStringView text) const
{
    auto answerable = check_answerable(_impl != nullptr ? _impl->message : nullptr);
    SIHD_UNEXPECTED_RETURN(answerable);
    if (!dbus_validate_error_name(name.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus error name: {}", name));
    if (!dbus_validate_utf8(text.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "dbus string is not valid utf-8"));

    ::DBusMessage *reply = dbus_message_new_error(_impl->message, name.c_str(), text.c_str());
    if (reply == nullptr)
        return std::unexpected(Error(out_of_memory, "could not create dbus error return"));
    auto impl = std::make_unique<Impl>();
    impl->message = reply;
    return Message(std::move(impl));
}

std::string Message::path() const
{
    return _impl != nullptr ? string_or_empty(dbus_message_get_path(_impl->message)) : std::string();
}

std::string Message::interface() const
{
    return _impl != nullptr ? string_or_empty(dbus_message_get_interface(_impl->message)) : std::string();
}

std::string Message::member() const
{
    return _impl != nullptr ? string_or_empty(dbus_message_get_member(_impl->message)) : std::string();
}

std::string Message::sender() const
{
    return _impl != nullptr ? string_or_empty(dbus_message_get_sender(_impl->message)) : std::string();
}

std::string Message::error_name() const
{
    return _impl != nullptr ? string_or_empty(dbus_message_get_error_name(_impl->message)) : std::string();
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

Error Message::to_error() const
{
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
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));

    if (!connection._auto_start)
        forbid_auto_start(_impl->message);
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    ::DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection._impl->connection,
                                                                     _impl->message,
                                                                     timeout.milliseconds_int(),
                                                                     &dbus_error);
    if (reply == nullptr)
        return std::unexpected(dbus_error_take(dbus_error));
    auto impl = std::make_unique<Impl>();
    impl->message = reply;
    return Message(std::move(impl));
}

Result<std::vector<Message>>
    Message::call_all(Connection & connection, const std::vector<Message> & requests, Duration timeout)
{
    if (connection._impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));

    std::vector<PendingCall> pending_calls;
    pending_calls.reserve(requests.size());
    for (const Message & request : requests)
    {
        if (request._impl == nullptr)
            return std::unexpected(Error(not_initialized, "dbus message is empty"));
        if (!connection._auto_start)
            forbid_auto_start(request._impl->message);
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
        auto impl = std::make_unique<Impl>();
        impl->message = dbus_pending_call_steal_reply(pending.get());
        if (impl->message == nullptr)
            return std::unexpected(Error(io_error, "dbus call completed without a reply"));
        Message reply(std::move(impl));
        if (reply.type() == Type::error)
            return std::unexpected(reply.to_error());
        replies.emplace_back(std::move(reply));
    }
    return replies;
}

Result<void> Message::send(Connection & connection) const
{
    if (connection._impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if (!connection._auto_start)
        forbid_auto_start(_impl->message);
    if (!dbus_connection_send(connection._impl->connection, _impl->message, nullptr))
        return std::unexpected(Error(out_of_memory, "could not send dbus message"));
    dbus_connection_flush(connection._impl->connection);
    return {};
}

Result<std::optional<Message>> Message::receive(Connection & connection, Duration timeout)
{
    Connection::Impl *impl = connection._impl.get();
    if (impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));

    SteadyClock clock;
    const Timestamp deadline = clock.now() + timeout;
    while (true)
    {
        dbus_connection_read_write(impl->connection, 0);
        if (::DBusMessage *message = dbus_connection_pop_message(impl->connection))
        {
            auto message_impl = std::make_unique<Impl>();
            message_impl->message = message;
            return Message(std::move(message_impl));
        }
        if (!dbus_connection_get_is_connected(impl->connection))
            return std::unexpected(Error(closed, "dbus connection lost"));

        const Timestamp now = clock.now();
        if (now >= deadline)
            return std::nullopt;
        if (impl->poll.poll(Duration(deadline - now).milliseconds_int()) < 0)
            return std::unexpected(Error(io_error, "could not poll the dbus connection"));
        auto woken = impl->wake.read();
        SIHD_UNEXPECTED_RETURN(woken);
        if (*woken > 0)
            return std::nullopt;
    }
}

Result<void> Connection::open(Bus bus)
{
    this->close();
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    ::DBusConnection *connection = dbus_bus_get_private(bus == Bus::system ? DBUS_BUS_SYSTEM : DBUS_BUS_SESSION,
                                                        &dbus_error);
    if (connection == nullptr)
        return std::unexpected(dbus_error_take(dbus_error));
    auto impl = Impl::adopt(connection);
    SIHD_UNEXPECTED_RETURN(impl);
    _impl = std::move(*impl);
    return {};
}

Result<void> Connection::open(ZStringView address)
{
    this->close();
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    ::DBusConnection *connection = dbus_connection_open_private(address.c_str(), &dbus_error);
    if (connection == nullptr)
        return std::unexpected(dbus_error_take(dbus_error));
    auto impl = Impl::adopt(connection);
    SIHD_UNEXPECTED_RETURN(impl);
    if (!dbus_bus_register(connection, &dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    _impl = std::move(*impl);
    return {};
}

Result<void> Connection::request_name(ZStringView name)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    if (!dbus_validate_bus_name(name.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus name: {}", name));
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    const int reply = dbus_bus_request_name(_impl->connection, name.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE, &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    if (reply != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER && reply != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER)
        return std::unexpected(Error(already_exists, "dbus name {} is owned by another connection", name));
    return {};
}

Result<bool> Connection::name_has_owner(ZStringView name)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    if (!dbus_validate_bus_name(name.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus name: {}", name));
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    const dbus_bool_t owned = dbus_bus_name_has_owner(_impl->connection, name.c_str(), &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    return owned != 0;
}

Result<void> Connection::add_match(ZStringView rule)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    dbus_bus_add_match(_impl->connection, rule.c_str(), &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    return {};
}

Result<void> Connection::remove_match(ZStringView rule)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    dbus_bus_remove_match(_impl->connection, rule.c_str(), &dbus_error);
    if (dbus_error_is_set(&dbus_error))
        return std::unexpected(dbus_error_take(dbus_error));
    return {};
}

Result<void> Connection::interrupt()
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    return _impl->wake.notify();
}

# else

struct Connection::Impl
{
};

struct Message::Impl
{
};

struct Iter::Impl
{
};

Iter::Iter(const Message &) {}

char Iter::type() const
{
    return '\0';
}

bool Iter::next()
{
    return false;
}

template <BasicType T>
Result<T> Iter::get() const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Iter> Iter::recurse() const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Message> Message::method_call(ZStringView, ZStringView, ZStringView, ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Message> Message::call(Connection &, Duration) const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::open(Bus)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Message> Message::signal(ZStringView, ZStringView, ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
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

std::string Message::error_name() const
{
    return {};
}

Message::Type Message::type() const
{
    return Type::invalid;
}

Error Message::to_error() const
{
    return Error(not_supported, "dbus is not supported on this platform");
}

Result<std::vector<Message>> Message::call_all(Connection &, const std::vector<Message> &, Duration)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Message::send(Connection &) const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::add_match(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::remove_match(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<std::optional<Message>> Message::receive(Connection &, Duration)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::interrupt()
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

struct Appender::Impl
{
};

Appender::Appender(Message &) {}

Result<void> Appender::add(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Appender::add_object_path(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

template <FixedType T>
Result<void> Appender::add(const T &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

template <BasicType T>
Result<void> Appender::add_array(const std::vector<T> &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Appender> Appender::open_array(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Appender> Appender::open_variant(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Appender> Appender::open_struct()
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Appender> Appender::open_dict_entry()
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Appender::close(Appender &&)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Message> Message::method_return() const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Message> Message::error_return(ZStringView, ZStringView) const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::open(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::request_name(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<bool> Connection::name_has_owner(ZStringView)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

# endif

namespace
{

constexpr ZStringView properties_interface = "org.freedesktop.DBus.Properties";

Result<Message> property_reply(Connection & connection,
                               ZStringView destination,
                               ZStringView path,
                               ZStringView interface,
                               ZStringView name,
                               Duration timeout)
{
    auto request = Message::properties_get(destination, path, interface, name);
    SIHD_UNEXPECTED_RETURN(request);
    return request->call(connection, timeout);
}

} // namespace

Result<void>
    Appender::add_entry(ZStringView key, ZStringView signature, const std::function<Result<void>(Appender &)> & fill)
{
    auto entry = this->open_dict_entry();
    SIHD_UNEXPECTED_RETURN(entry);
    auto key_added = entry->add(key);
    SIHD_UNEXPECTED_RETURN(key_added);
    auto variant = entry->open_variant(signature);
    SIHD_UNEXPECTED_RETURN(variant);
    auto filled = fill(*variant);
    SIHD_UNEXPECTED_RETURN(filled);
    auto variant_closed = entry->close(std::move(*variant));
    SIHD_UNEXPECTED_RETURN(variant_closed);
    return this->close(std::move(*entry));
}

Result<Message>
    Message::properties_get(ZStringView destination, ZStringView path, ZStringView interface, ZStringView name)
{
    auto request = Message::method_call(destination, path, properties_interface, "Get");
    SIHD_UNEXPECTED_RETURN(request);
    auto interface_added = request->add(interface);
    SIHD_UNEXPECTED_RETURN(interface_added);
    auto name_added = request->add(name);
    SIHD_UNEXPECTED_RETURN(name_added);
    return request;
}

Result<Message> Message::properties_get_all(ZStringView destination, ZStringView path, ZStringView interface)
{
    auto request = Message::method_call(destination, path, properties_interface, "GetAll");
    SIHD_UNEXPECTED_RETURN(request);
    auto interface_added = request->add(interface);
    SIHD_UNEXPECTED_RETURN(interface_added);
    return request;
}

Result<std::optional<Iter>> Message::changed_property(std::string_view interface, std::string_view name) const
{
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

Result<Message> Message::receive_until(Connection & connection,
                                       Duration timeout,
                                       const std::function<Result<bool>(const Message &)> & accepted)
{
    SteadyClock clock;
    const Timestamp deadline = clock.now() + timeout;
    while (true)
    {
        const Timestamp now = clock.now();
        if (now >= deadline)
            return std::unexpected(Error(ErrorCode::timeout, "dbus receive timed out"));
        auto received = Message::receive(connection, Duration(deadline - now));
        SIHD_UNEXPECTED_RETURN(received);
        if (!received->has_value())
            continue;
        auto taken = accepted(**received);
        SIHD_UNEXPECTED_RETURN(taken);
        if (*taken)
            return std::move(**received);
    }
}

template <BasicType T>
Result<T> get_property(Connection & connection,
                       ZStringView destination,
                       ZStringView path,
                       ZStringView interface,
                       ZStringView name,
                       Duration timeout)
{
    auto reply = property_reply(connection, destination, path, interface, name, timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    auto value = Iter(*reply).recurse();
    SIHD_UNEXPECTED_RETURN(value);
    return value->get<T>();
}

template <BasicType T>
Result<std::vector<T>> get_array_property(Connection & connection,
                                          ZStringView destination,
                                          ZStringView path,
                                          ZStringView interface,
                                          ZStringView name,
                                          Duration timeout)
{
    auto reply = property_reply(connection, destination, path, interface, name, timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    auto value = Iter(*reply).recurse();
    SIHD_UNEXPECTED_RETURN(value);
    return value->get_array<T>();
}

std::string
    signal_rule(std::string_view sender, std::string_view path, std::string_view interface, std::string_view member)
{
    return fmt::format("type='signal',sender='{}',path='{}',interface='{}',member='{}'",
                       sender,
                       path,
                       interface,
                       member);
}

std::string properties_changed_rule(std::string_view sender, std::string_view path, std::string_view interface)
{
    return fmt::format("{},arg0='{}'", signal_rule(sender, path, properties_interface, "PropertiesChanged"), interface);
}

bool has_error_name(const Error & error, std::string_view name)
{
    return error.message.starts_with(name) && error.message.substr(name.size()).starts_with(':');
}

Appender::Appender(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Appender::Appender(Appender && other) = default;

Appender & Appender::operator=(Appender && other) = default;

Appender::~Appender() = default;

Result<void> Message::add(ZStringView value)
{
    return Appender(*this).add(value);
}

Result<void> Message::add_object_path(ZStringView path)
{
    return Appender(*this).add_object_path(path);
}

template <FixedType T>
Result<void> Message::add(const T & value)
{
    return Appender(*this).add(value);
}

Iter::Iter(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Iter::Iter(Iter && other) = default;

Iter & Iter::operator=(Iter && other) = default;

Iter::~Iter() = default;

Message::Message(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Message::Message(Message && other) = default;

Message & Message::operator=(Message && other) = default;

Message::~Message() = default;

template <BasicType T>
Result<std::vector<T>> Iter::get_array() const
{
    auto items = this->recurse();
    SIHD_UNEXPECTED_RETURN(items);
    if (this->type() != 'a')
        return std::unexpected(Error(invalid_argument, "dbus type '{}' is not an array", this->type()));

    std::vector<T> values;
    for (; items->type() != '\0'; items->next())
    {
        auto value = items->get<T>();
        SIHD_UNEXPECTED_RETURN(value);
        values.emplace_back(std::move(*value));
    }
    return values;
}

template <BasicType Key>
Result<void> Iter::for_each_entry(const std::function<Result<void>(const Key &, Iter &)> & callback) const
{
    if (this->type() != 'a')
        return std::unexpected(Error(invalid_argument, "dbus type '{}' is not a dict", this->type()));
    auto entries = this->recurse();
    SIHD_UNEXPECTED_RETURN(entries);

    for (; entries->type() != '\0'; entries->next())
    {
        auto entry = entries->recurse();
        SIHD_UNEXPECTED_RETURN(entry);
        auto key = entry->get<Key>();
        SIHD_UNEXPECTED_RETURN(key);
        entry->next();

        auto value = entry->type() == 'v' ? entry->recurse() : Result<Iter>(std::move(*entry));
        SIHD_UNEXPECTED_RETURN(value);
        auto done = callback(*key, *value);
        SIHD_UNEXPECTED_RETURN(done);
    }
    return {};
}

Connection::Connection() = default;

Connection::Connection(Connection && other) = default;

Connection & Connection::operator=(Connection && other) = default;

Connection::~Connection() = default;

bool Connection::is_open() const
{
    return _impl != nullptr;
}

void Connection::close()
{
    _impl.reset();
}

void Connection::disable_auto_start()
{
    _auto_start = false;
}

template Result<bool> Iter::get<bool>() const;
template Result<uint8_t> Iter::get<uint8_t>() const;
template Result<int16_t> Iter::get<int16_t>() const;
template Result<uint16_t> Iter::get<uint16_t>() const;
template Result<int32_t> Iter::get<int32_t>() const;
template Result<uint32_t> Iter::get<uint32_t>() const;
template Result<std::string> Iter::get<std::string>() const;

template Result<std::vector<bool>> Iter::get_array<bool>() const;
template Result<std::vector<uint8_t>> Iter::get_array<uint8_t>() const;
template Result<std::vector<int16_t>> Iter::get_array<int16_t>() const;
template Result<std::vector<uint16_t>> Iter::get_array<uint16_t>() const;
template Result<std::vector<int32_t>> Iter::get_array<int32_t>() const;
template Result<std::vector<uint32_t>> Iter::get_array<uint32_t>() const;
template Result<std::vector<std::string>> Iter::get_array<std::string>() const;

template Result<void> Iter::for_each_entry<bool>(const std::function<Result<void>(const bool &, Iter &)> &) const;
template Result<void> Iter::for_each_entry<uint8_t>(const std::function<Result<void>(const uint8_t &, Iter &)> &) const;
template Result<void> Iter::for_each_entry<int16_t>(const std::function<Result<void>(const int16_t &, Iter &)> &) const;
template Result<void>
    Iter::for_each_entry<uint16_t>(const std::function<Result<void>(const uint16_t &, Iter &)> &) const;
template Result<void> Iter::for_each_entry<int32_t>(const std::function<Result<void>(const int32_t &, Iter &)> &) const;
template Result<void>
    Iter::for_each_entry<uint32_t>(const std::function<Result<void>(const uint32_t &, Iter &)> &) const;
template Result<void>
    Iter::for_each_entry<std::string>(const std::function<Result<void>(const std::string &, Iter &)> &) const;

template Result<void> Message::add<bool>(const bool &);
template Result<void> Message::add<uint8_t>(const uint8_t &);
template Result<void> Message::add<int16_t>(const int16_t &);
template Result<void> Message::add<uint16_t>(const uint16_t &);
template Result<void> Message::add<int32_t>(const int32_t &);
template Result<void> Message::add<uint32_t>(const uint32_t &);

template Result<void> Appender::add<bool>(const bool &);
template Result<void> Appender::add<uint8_t>(const uint8_t &);
template Result<void> Appender::add<int16_t>(const int16_t &);
template Result<void> Appender::add<uint16_t>(const uint16_t &);
template Result<void> Appender::add<int32_t>(const int32_t &);
template Result<void> Appender::add<uint32_t>(const uint32_t &);

template Result<void> Appender::add_array<bool>(const std::vector<bool> &);
template Result<void> Appender::add_array<uint8_t>(const std::vector<uint8_t> &);
template Result<void> Appender::add_array<int16_t>(const std::vector<int16_t> &);
template Result<void> Appender::add_array<uint16_t>(const std::vector<uint16_t> &);
template Result<void> Appender::add_array<int32_t>(const std::vector<int32_t> &);
template Result<void> Appender::add_array<uint32_t>(const std::vector<uint32_t> &);
template Result<void> Appender::add_array<std::string>(const std::vector<std::string> &);

template Result<bool> get_property<bool>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<uint8_t>
    get_property<uint8_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<int16_t>
    get_property<int16_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<uint16_t>
    get_property<uint16_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<int32_t>
    get_property<int32_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<uint32_t>
    get_property<uint32_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<std::string>
    get_property<std::string>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);

template Result<std::vector<bool>>
    get_array_property<bool>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<std::vector<uint8_t>>
    get_array_property<uint8_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<std::vector<int16_t>>
    get_array_property<int16_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<std::vector<uint16_t>>
    get_array_property<uint16_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<std::vector<int32_t>>
    get_array_property<int32_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<std::vector<uint32_t>>
    get_array_property<uint32_t>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);
template Result<std::vector<std::string>>
    get_array_property<std::string>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);

} // namespace sihd::sys::linux_::dbus

#endif
