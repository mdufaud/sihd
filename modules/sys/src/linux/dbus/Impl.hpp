#ifndef __SIHD_SYS_LINUX_DBUS_IMPL_HPP__
#define __SIHD_SYS_LINUX_DBUS_IMPL_HPP__

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <dbus/dbus.h>

# include <sihd/sys/Poll.hpp>
# include <sihd/sys/linux/EventFd.hpp>
#endif

#define SIHD_DBUS_FOR_EACH_FIXED_TYPE(X)                                                                               \
    X(bool) X(uint8_t) X(int16_t) X(uint16_t) X(int32_t) X(uint32_t) X(int64_t) X(uint64_t) X(double)

#define SIHD_DBUS_FOR_EACH_BASIC_TYPE(X) SIHD_DBUS_FOR_EACH_FIXED_TYPE(X) X(std::string)

namespace sihd::sys::linux_::dbus
{

inline constexpr sihd::util::ZStringView properties_interface = "org.freedesktop.DBus.Properties";

sihd::util::Error empty_message();
sihd::util::Error connection_not_open();
std::unexpected<sihd::util::Error> unsupported_platform();

// the first message matches accepts, matches may consume the ones it refuses; what names the wait in the errors
sihd::util::Result<Message> receive_matching(Connection & connection,
                                             sihd::util::Duration timeout,
                                             std::string_view what,
                                             const std::function<sihd::util::Result<bool>(Message &)> & matches);

// the dbus wire type codes, libdbus not being there on every platform
template <typename T>
constexpr char dbus_type_of()
{
    if constexpr (std::is_same_v<T, bool>)
        return 'b';
    else if constexpr (std::is_same_v<T, uint8_t>)
        return 'y';
    else if constexpr (std::is_same_v<T, int16_t>)
        return 'n';
    else if constexpr (std::is_same_v<T, uint16_t>)
        return 'q';
    else if constexpr (std::is_same_v<T, int32_t>)
        return 'i';
    else if constexpr (std::is_same_v<T, uint32_t>)
        return 'u';
    else if constexpr (std::is_same_v<T, int64_t>)
        return 'x';
    else if constexpr (std::is_same_v<T, uint64_t>)
        return 't';
    else if constexpr (std::is_same_v<T, std::string>)
        return 's';
    else
        return 'd';
}

#if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

sihd::util::Error dbus_error_take(::DBusError & dbus_error);
sihd::util::ZStringView error_name_of(sihd::util::ErrorCode code);

std::string string_or_empty(const char *str);

struct Connection::Impl
{
        ::DBusConnection *connection = nullptr;
        // receive() waits on the connection socket and on this wake up counter
        EventFd wake;
        // a flush in another thread can queue what it reads from the socket: receive() must look at the queue again
        EventFd queued;
        Poll poll {3};

        ~Impl();

        sihd::util::Result<void> adopt(::DBusConnection *opened, ::DBusError & dbus_error);
};

struct Message::Impl
{
        // never null
        ::DBusMessage *message;

        explicit Impl(::DBusMessage *adopted);
        ~Impl();

        static Message wrap(::DBusMessage *message);
        static sihd::util::Result<Message> created(::DBusMessage *message, std::string_view what);
};

#else

struct Connection::Impl
{
};

struct Message::Impl
{
};

#endif

} // namespace sihd::sys::linux_::dbus

#endif
