#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <algorithm>
# include <limits>
# include <string>
# include <type_traits>
# include <utility>

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
#  include <dbus/dbus.h>

#  include <sihd/sys/Poll.hpp>
#  include <sihd/sys/linux/EventFd.hpp>
#  include <sihd/util/Clocks.hpp>
# endif

# include <sihd/sys/linux/dbus.hpp>

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

Error dbus_error_take(::DBusError & dbus_error)
{
    Error error(io_error, "{}: {}", dbus_error.name, dbus_error.message);
    dbus_error_free(&dbus_error);
    return error;
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

int timeout_ms(Duration timeout)
{
    return static_cast<int>(std::min<time::UnixTime>(timeout.milliseconds(), std::numeric_limits<int>::max()));
}

Result<void> validate_names(const std::string & path, const std::string & interface, const std::string & member)
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

Result<void> Appender::add(const std::string & value)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    if (!dbus_validate_utf8(value.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "dbus string is not valid utf-8"));
    const char *str = value.c_str();
    return _impl->append(DBUS_TYPE_STRING, &str);
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

Result<Appender> Appender::open_array(const std::string & element_signature)
{
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
    const std::string type = "a" + element_signature;
    if (!dbus_signature_validate_single(type.c_str(), nullptr))
        return std::unexpected(Error(invalid_argument, "invalid dbus array element signature: {}", element_signature));
    auto next = _impl->expect(type);
    SIHD_UNEXPECTED_RETURN(next);

    auto opened = _impl->open_child(DBUS_TYPE_ARRAY,
                                    element_signature.c_str(),
                                    *next,
                                    Impl::constrained_to(element_signature, true));
    SIHD_UNEXPECTED_RETURN(opened);
    return Appender(std::move(*opened));
}

Result<Appender> Appender::open_variant(const std::string & value_signature)
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
                                    Impl::constrained_to(value_signature, false));
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

Result<Message> Message::method_call(const std::string & destination,
                                     const std::string & path,
                                     const std::string & interface,
                                     const std::string & member)
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

Result<Message> Message::signal(const std::string & path, const std::string & interface, const std::string & member)
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

Result<Message> Message::error_return(const std::string & name, const std::string & text) const
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

Result<Message> Message::call(Connection & connection, Duration timeout) const
{
    if (connection._impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));

    ::DBusError dbus_error;
    dbus_error_init(&dbus_error);
    ::DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection._impl->connection,
                                                                     _impl->message,
                                                                     timeout_ms(timeout),
                                                                     &dbus_error);
    if (reply == nullptr)
        return std::unexpected(dbus_error_take(dbus_error));
    auto impl = std::make_unique<Impl>();
    impl->message = reply;
    return Message(std::move(impl));
}

Result<void> Message::send(Connection & connection) const
{
    if (connection._impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus connection is not open"));
    if (_impl == nullptr)
        return std::unexpected(Error(not_initialized, "dbus message is empty"));
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
        if (impl->poll.poll(timeout_ms(Duration(deadline - now))) < 0)
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

Result<void> Connection::open(const std::string & address)
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

Result<void> Connection::request_name(const std::string & name)
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

Result<void> Connection::add_match(const std::string & rule)
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

Result<void> Connection::remove_match(const std::string & rule)
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

Result<Message> Message::method_call(const std::string &, const std::string &, const std::string &, const std::string &)
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

Result<Message> Message::signal(const std::string &, const std::string &, const std::string &)
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

Result<void> Message::send(Connection &) const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::add_match(const std::string &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::remove_match(const std::string &)
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

Result<void> Appender::add(const std::string &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

template <FixedType T>
Result<void> Appender::add(const T &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Appender> Appender::open_array(const std::string &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<Appender> Appender::open_variant(const std::string &)
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

Result<Message> Message::error_return(const std::string &, const std::string &) const
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::open(const std::string &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

Result<void> Connection::request_name(const std::string &)
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

# endif

Appender::Appender(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

Appender::Appender(Appender && other) = default;

Appender & Appender::operator=(Appender && other) = default;

Appender::~Appender() = default;

Result<void> Message::add(const std::string & value)
{
    return Appender(*this).add(value);
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

} // namespace sihd::sys::linux_::dbus

#endif
