#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <string>
# include <utility>

# include <sihd/sys/File.hpp>
# include <sihd/sys/linux/dbus.hpp>
# include <sihd/util/time.hpp>

# include "internal/desktop_bus.hpp"
#endif

#include <sihd/sys/desktop.hpp>

#include "../internal/desktop.hpp"

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace dbus = sihd::sys::linux_::dbus;

namespace
{

constexpr ZStringView open_uri_interface = "org.freedesktop.portal.OpenURI";
constexpr ZStringView settings_interface = "org.freedesktop.portal.Settings";
constexpr ZStringView response_rule = "type='signal',sender='org.freedesktop.portal.Desktop',"
                                      "interface='org.freedesktop.portal.Request',member='Response'";
constexpr ZStringView unknown_method_error = "org.freedesktop.DBus.Error.UnknownMethod";
constexpr ZStringView setting_not_found_error = "org.freedesktop.portal.Error.NotFound";
constexpr ZStringView appearance_namespace = "org.freedesktop.appearance";
constexpr ZStringView color_scheme_key = "color-scheme";
// a choice of application can be on screen, the user answers it
constexpr Duration response_timeout(time::sec(30));

constexpr uint32_t response_success = 0;
constexpr uint32_t response_cancelled = 1;

Result<void> append_no_options(dbus::Appender & body)
{
    auto options = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(options);
    return body.close(std::move(*options));
}

// the portal cancels the request of a client that disconnects: the connection stays until the response
Result<void> call_and_wait_response(const dbus::Message & request)
{
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::session);
    SIHD_UNEXPECTED_RETURN(opened);
    // set before the call: the response can come right behind its reply
    auto matched = connection.add_match(response_rule);
    SIHD_UNEXPECTED_RETURN(matched);
    auto reply = request.call(connection, internal::dbus_call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    auto handle = dbus::Iter(*reply).get<std::string>();
    SIHD_UNEXPECTED_RETURN(handle);

    auto response = dbus::Message::receive_until(connection,
                                                 response_timeout,
                                                 [&handle](const dbus::Message & message) -> Result<bool> {
                                                     return message.member() == "Response" && message.path() == *handle;
                                                 });
    SIHD_UNEXPECTED_RETURN(response);
    auto code = dbus::Iter(*response).get<uint32_t>();
    SIHD_UNEXPECTED_RETURN(code);
    if (*code == response_success)
        return {};
    if (*code == response_cancelled)
        return std::unexpected(Error(permission_denied, "the desktop portal request was cancelled"));
    return std::unexpected(Error(io_error, "the desktop portal request failed"));
}

Result<void> open_by_fd(ZStringView member, ZStringView path)
{
    File file;
    auto file_opened = file.open(std::string(path), "r");
    SIHD_UNEXPECTED_RETURN(file_opened);

    auto request = dbus::Message::method_call(internal::portal_service,
                                              internal::portal_path,
                                              open_uri_interface,
                                              member);
    SIHD_UNEXPECTED_RETURN(request);
    dbus::Appender body(*request);
    auto added = body.add(ZStringView(""));
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add_unix_fd(file.fd());
    SIHD_UNEXPECTED_RETURN(added);
    added = append_no_options(body);
    SIHD_UNEXPECTED_RETURN(added);
    return call_and_wait_response(*request);
}

Result<dbus::Message> read_setting(dbus::Connection & connection, ZStringView member)
{
    auto request = dbus::Message::method_call(internal::portal_service,
                                              internal::portal_path,
                                              settings_interface,
                                              member);
    SIHD_UNEXPECTED_RETURN(request);
    auto added = request->add(appearance_namespace);
    SIHD_UNEXPECTED_RETURN(added);
    added = request->add(color_scheme_key);
    SIHD_UNEXPECTED_RETURN(added);
    return request->call(connection, internal::dbus_call_timeout);
}

Result<uint32_t> unwrapped_uint32(const dbus::Message & message)
{
    // ReadOne wraps the value in one variant, the Read of old portals in two
    dbus::Iter iter(message);
    while (iter.type() == 'v')
    {
        auto inner = iter.recurse();
        SIHD_UNEXPECTED_RETURN(inner);
        iter = std::move(*inner);
    }
    return iter.get<uint32_t>();
}

} // namespace

namespace internal
{

Result<void> desktop_open_uri(ZStringView uri)
{
    auto request = dbus::Message::method_call(internal::portal_service,
                                              internal::portal_path,
                                              open_uri_interface,
                                              "OpenURI");
    SIHD_UNEXPECTED_RETURN(request);
    dbus::Appender body(*request);
    auto added = body.add(ZStringView(""));
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add(uri);
    SIHD_UNEXPECTED_RETURN(added);
    added = append_no_options(body);
    SIHD_UNEXPECTED_RETURN(added);
    return call_and_wait_response(*request);
}

Result<void> desktop_open_file(ZStringView path)
{
    return open_by_fd("OpenFile", path);
}

Result<void> desktop_reveal(ZStringView path)
{
    return open_by_fd("OpenDirectory", path);
}

} // namespace internal

namespace desktop
{

Result<ColorScheme> color_scheme()
{
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::session);
    SIHD_UNEXPECTED_RETURN(opened);

    auto reply = read_setting(connection, "ReadOne");
    if (!reply.has_value() && dbus::has_error_name(reply.error(), unknown_method_error))
        reply = read_setting(connection, "Read");
    if (!reply.has_value() && dbus::has_error_name(reply.error(), setting_not_found_error))
        return ColorScheme::no_preference;
    SIHD_UNEXPECTED_RETURN(reply);

    auto scheme = unwrapped_uint32(*reply);
    SIHD_UNEXPECTED_RETURN(scheme);
    switch (*scheme)
    {
        case 1:
            return ColorScheme::dark;
        case 2:
            return ColorScheme::light;
        default:
            return ColorScheme::no_preference;
    }
}

} // namespace desktop

#else

namespace
{

std::unexpected<Error> unsupported()
{
    return std::unexpected(Error(not_supported, "desktop services are not supported on this platform"));
}

} // namespace

namespace internal
{

Result<void> desktop_open_uri(ZStringView)
{
    return unsupported();
}

Result<void> desktop_open_file(ZStringView)
{
    return unsupported();
}

Result<void> desktop_reveal(ZStringView)
{
    return unsupported();
}

} // namespace internal

namespace desktop
{

Result<ColorScheme> color_scheme()
{
    return unsupported();
}

} // namespace desktop

#endif

} // namespace sihd::sys
