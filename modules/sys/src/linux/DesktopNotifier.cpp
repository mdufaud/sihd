#include <string>
#include <utility>

#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <algorithm>
# include <cstdint>
# include <optional>

# include <sihd/sys/linux/dbus.hpp>
# include <sihd/util/str.hpp>
# include <sihd/util/time.hpp>
#endif

#include <sihd/sys/DesktopNotifier.hpp>

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace dbus = sihd::sys::linux_::dbus;

namespace
{

constexpr ZStringView notifications_service = "org.freedesktop.Notifications";
constexpr ZStringView notifications_path = "/org/freedesktop/Notifications";
constexpr Duration call_timeout(time::sec(5));

uint8_t urgency_level(DesktopNotifier::Urgency urgency)
{
    switch (urgency)
    {
        case DesktopNotifier::Urgency::low:
            return 0;
        case DesktopNotifier::Urgency::critical:
            return 2;
        default:
            return 1;
    }
}

int32_t expire_timeout(const std::optional<Duration> & timeout)
{
    if (!timeout.has_value())
        return -1;
    return timeout->milliseconds_int();
}

Result<void> append_hint_array(dbus::Appender & body, const std::string & app_id, DesktopNotifier::Urgency urgency)
{
    auto hints = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(hints);
    auto urgency_added = hints->add_entry("urgency", "y", [urgency](dbus::Appender & value) {
        return value.add(urgency_level(urgency));
    });
    SIHD_UNEXPECTED_RETURN(urgency_added);
    if (!app_id.empty())
    {
        auto entry_added = hints->add_entry("desktop-entry", "s", [&app_id](dbus::Appender & value) {
            return value.add(app_id);
        });
        SIHD_UNEXPECTED_RETURN(entry_added);
    }
    return body.close(std::move(*hints));
}

Result<void> append_notification(dbus::Message & request,
                                 const std::string & app_name,
                                 const std::string & app_id,
                                 const DesktopNotifier::Notification & notification,
                                 bool body_markup)
{
    dbus::Appender body(request);
    auto added = body.add(app_name);
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add(uint32_t {0});
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add(notification.icon);
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add(notification.summary);
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add(body_markup ? str::escape_xml(notification.body) : notification.body);
    SIHD_UNEXPECTED_RETURN(added);

    auto actions = body.open_array("s");
    SIHD_UNEXPECTED_RETURN(actions);
    added = body.close(std::move(*actions));
    SIHD_UNEXPECTED_RETURN(added);
    added = append_hint_array(body, app_id, notification.urgency);
    SIHD_UNEXPECTED_RETURN(added);
    return body.add(expire_timeout(notification.timeout));
}

} // namespace

struct DesktopNotifier::Impl
{
        dbus::Connection connection;
        std::string app_name;
        std::string app_id;
        std::optional<bool> body_markup;

        Result<dbus::Message> new_call(const std::string & member)
        {
            if (!connection.is_open())
            {
                body_markup.reset();
                auto opened = connection.open(dbus::Bus::session);
                SIHD_UNEXPECTED_RETURN(opened);
            }
            return dbus::Message::method_call(notifications_service, notifications_path, notifications_service, member);
        }

        // a failed call drops the connection: the next one reconnects to a restarted bus or server
        Result<dbus::Message> call(const dbus::Message & request)
        {
            auto reply = request.call(connection, call_timeout);
            if (!reply.has_value())
                connection.close();
            return reply;
        }

        Result<bool> has_body_markup()
        {
            if (body_markup.has_value())
                return *body_markup;
            auto request = this->new_call("GetCapabilities");
            SIHD_UNEXPECTED_RETURN(request);
            auto reply = this->call(*request);
            SIHD_UNEXPECTED_RETURN(reply);
            auto capabilities = dbus::Iter(*reply).get_array<std::string>();
            SIHD_UNEXPECTED_RETURN(capabilities);
            body_markup = std::ranges::find(*capabilities, "body-markup") != capabilities->end();
            return *body_markup;
        }
};

Result<uint32_t> DesktopNotifier::notify(const Notification & notification)
{
    auto valid_timeout = DesktopNotifier::check_timeout(notification.timeout);
    SIHD_UNEXPECTED_RETURN(valid_timeout);
    auto body_markup = _impl->has_body_markup();
    SIHD_UNEXPECTED_RETURN(body_markup);
    auto request = _impl->new_call("Notify");
    SIHD_UNEXPECTED_RETURN(request);
    auto appended = append_notification(*request, _impl->app_name, _impl->app_id, notification, *body_markup);
    SIHD_UNEXPECTED_RETURN(appended);

    auto reply = _impl->call(*request);
    SIHD_UNEXPECTED_RETURN(reply);
    return dbus::Iter(*reply).get<uint32_t>();
}

Result<void> DesktopNotifier::close(uint32_t id)
{
    auto request = _impl->new_call("CloseNotification");
    SIHD_UNEXPECTED_RETURN(request);
    auto added = request->add(id);
    SIHD_UNEXPECTED_RETURN(added);
    auto reply = _impl->call(*request);
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

#else

struct DesktopNotifier::Impl
{
        std::string app_name;
        std::string app_id;
};

Result<uint32_t> DesktopNotifier::notify(const Notification &)
{
    return std::unexpected(Error(not_supported, "desktop notifications are not supported on this platform"));
}

Result<void> DesktopNotifier::close(uint32_t)
{
    return std::unexpected(Error(not_supported, "desktop notifications are not supported on this platform"));
}

#endif

DesktopNotifier::DesktopNotifier(): _impl(std::make_unique<Impl>()) {}

DesktopNotifier::DesktopNotifier(DesktopNotifier && other) = default;

DesktopNotifier & DesktopNotifier::operator=(DesktopNotifier && other) = default;

DesktopNotifier::~DesktopNotifier() = default;

void DesktopNotifier::set_app_name(const std::string & name)
{
    _impl->app_name = name;
}

void DesktopNotifier::set_app_id(const std::string & id)
{
    _impl->app_id = id;
}

} // namespace sihd::sys
