#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

# include "desktop_bus.hpp"
# include "login1.hpp"

namespace sihd::sys::internal
{

using namespace sihd::util;

namespace dbus = sihd::sys::linux_::dbus;

Result<dbus::UnixFd>
    login1_inhibit_sleep(dbus::Connection & connection, ZStringView who, ZStringView why, ZStringView mode)
{
    auto request = dbus::Message::method_call("org.freedesktop.login1",
                                              "/org/freedesktop/login1",
                                              "org.freedesktop.login1.Manager",
                                              "Inhibit");
    SIHD_UNEXPECTED_RETURN(request);
    auto added = request->add(ZStringView("sleep"));
    SIHD_UNEXPECTED_RETURN(added);
    added = request->add(who);
    SIHD_UNEXPECTED_RETURN(added);
    added = request->add(why);
    SIHD_UNEXPECTED_RETURN(added);
    added = request->add(mode);
    SIHD_UNEXPECTED_RETURN(added);
    auto reply = request->call(connection, dbus_call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return dbus::Iter(*reply).get_unix_fd();
}

} // namespace sihd::sys::internal

#endif
