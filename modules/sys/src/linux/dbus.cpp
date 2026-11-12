#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <array>

# include <fmt/format.h>

# include "dbus/Impl.hpp"

namespace sihd::sys::linux_::dbus
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace
{

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

struct ErrorMapping
{
        const char *name;
        ErrorCode code;
};

// one table both ways, first match when encoding: a timeout leaves as TimedOut, all three read back as timeout
constexpr std::array error_mappings = {
    ErrorMapping {DBUS_ERROR_ACCESS_DENIED, permission_denied},
    ErrorMapping {DBUS_ERROR_INVALID_ARGS, invalid_argument},
    ErrorMapping {DBUS_ERROR_NOT_SUPPORTED, not_supported},
    ErrorMapping {DBUS_ERROR_TIMED_OUT, ErrorCode::timeout},
    ErrorMapping {DBUS_ERROR_NO_REPLY, ErrorCode::timeout},
    ErrorMapping {DBUS_ERROR_TIMEOUT, ErrorCode::timeout},
};

ErrorCode code_of(const ::DBusError & dbus_error)
{
    for (const ErrorMapping & mapping : error_mappings)
    {
        if (dbus_error_has_name(&dbus_error, mapping.name))
            return mapping.code;
    }
    return io_error;
}

# endif

Result<Message> property_reply(Connection & connection,
                               ZStringView destination,
                               ZStringView path,
                               ZStringView interface,
                               ZStringView name,
                               Duration timeout)
{
    Message request;
    auto built = request.properties_get(destination, path, interface, name);
    SIHD_UNEXPECTED_RETURN(built);
    return request.call(connection, timeout);
}

} // namespace

Error empty_message()
{
    return Error(not_initialized, "dbus message is empty");
}

Error connection_not_open()
{
    return Error(not_initialized, "dbus connection is not open");
}

std::unexpected<Error> unsupported_platform()
{
    return std::unexpected(Error(not_supported, "dbus is not supported on this platform"));
}

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

std::string string_or_empty(const char *str)
{
    return str != nullptr ? std::string(str) : std::string();
}

Error dbus_error_take(::DBusError & dbus_error)
{
    Error error(code_of(dbus_error), "{}: {}", dbus_error.name, dbus_error.message);
    dbus_error_free(&dbus_error);
    return error;
}

ZStringView error_name_of(ErrorCode code)
{
    for (const ErrorMapping & mapping : error_mappings)
    {
        if (mapping.code == code)
            return mapping.name;
    }
    return DBUS_ERROR_FAILED;
}

# endif

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

template <BasicType T>
Result<void> set_property(Connection & connection,
                          ZStringView destination,
                          ZStringView path,
                          ZStringView interface,
                          ZStringView name,
                          const T & value,
                          Duration timeout)
{
    Message request;
    auto built = request.method_call(destination, path, properties_interface, "Set");
    SIHD_UNEXPECTED_RETURN(built);
    Appender body(request);
    auto interface_added = body.add(interface);
    SIHD_UNEXPECTED_RETURN(interface_added);
    auto name_added = body.add(name);
    SIHD_UNEXPECTED_RETURN(name_added);

    auto variant = body.open_variant(std::string(1, dbus_type_of<T>()));
    SIHD_UNEXPECTED_RETURN(variant);
    auto value_added = variant->add(value);
    SIHD_UNEXPECTED_RETURN(value_added);
    auto variant_closed = body.close(std::move(*variant));
    SIHD_UNEXPECTED_RETURN(variant_closed);

    auto reply = request.call(connection, timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
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

# define SIHD_DBUS_INSTANTIATE(T)                                                                                      \
     template Result<T> get_property<T>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, Duration);   \
     template Result<std::vector<T>> get_array_property<T>(Connection &,                                               \
                                                           ZStringView,                                                \
                                                           ZStringView,                                                \
                                                           ZStringView,                                                \
                                                           ZStringView,                                                \
                                                           Duration);                                                  \
     template Result<void>                                                                                             \
         set_property<T>(Connection &, ZStringView, ZStringView, ZStringView, ZStringView, const T &, Duration);
SIHD_DBUS_FOR_EACH_BASIC_TYPE(SIHD_DBUS_INSTANTIATE)
# undef SIHD_DBUS_INSTANTIATE

} // namespace sihd::sys::linux_::dbus

#endif
