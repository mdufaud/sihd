#ifndef __SIHD_SYS_LINUX_DBUS_HPP__
#define __SIHD_SYS_LINUX_DBUS_HPP__

#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/linux/dbus/Appender.hpp>
#include <sihd/sys/linux/dbus/Connection.hpp>
#include <sihd/sys/linux/dbus/Iter.hpp>
#include <sihd/sys/linux/dbus/Message.hpp>
#include <sihd/sys/linux/dbus/ObjectServer.hpp>
#include <sihd/sys/linux/dbus/UnixFd.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::linux_::dbus
{

template <BasicType T>
sihd::util::Result<T> get_property(Connection & connection,
                                   sihd::util::ZStringView destination,
                                   sihd::util::ZStringView path,
                                   sihd::util::ZStringView interface,
                                   sihd::util::ZStringView name,
                                   sihd::util::Duration timeout);
template <BasicType T>
sihd::util::Result<std::vector<T>> get_array_property(Connection & connection,
                                                      sihd::util::ZStringView destination,
                                                      sihd::util::ZStringView path,
                                                      sihd::util::ZStringView interface,
                                                      sihd::util::ZStringView name,
                                                      sihd::util::Duration timeout);

std::string
    signal_rule(std::string_view sender, std::string_view path, std::string_view interface, std::string_view member);
std::string properties_changed_rule(std::string_view sender, std::string_view path, std::string_view interface);

// relies on Message::call() failing with "<error name>: <message>"
bool has_error_name(const sihd::util::Error & error, std::string_view name);

} // namespace sihd::sys::linux_::dbus

#endif
