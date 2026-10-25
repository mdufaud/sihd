#include <optional>

#include <sihd/wifi/Monitor.hpp>

#include "wlanapi.hpp"

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

Monitor::Event state_changed(State state)
{
    return Monitor::Event {.type = Monitor::Event::Type::state_changed, .state = state};
}

std::optional<Monitor::Event> to_event(const wlanapi::Notification & notification)
{
    switch (notification.code)
    {
        case wlan_notification_acm_scan_complete:
            return Monitor::Event {.type = Monitor::Event::Type::scan_done, .state = State::disconnected};
        case wlan_notification_acm_connection_start:
            return state_changed(State::connecting);
        case wlan_notification_acm_connection_complete:
            return state_changed(notification.reason == WLAN_REASON_CODE_SUCCESS ? State::connected
                                                                                 : State::disconnected);
        case wlan_notification_acm_connection_attempt_fail:
        case wlan_notification_acm_disconnected:
            return state_changed(State::disconnected);
        case wlan_notification_acm_disconnecting:
            return state_changed(State::disconnecting);
        default:
            return std::nullopt;
    }
}

} // namespace

struct Monitor::Impl
{
        wlanapi::Session session;
        GUID guid {};
        bool listening = false;
};

Monitor::Monitor(): _impl(std::make_unique<Impl>()) {}

Monitor::~Monitor() = default;

Result<void> Monitor::open(const std::string & interface_name)
{
    if (_impl->listening)
        return std::unexpected(Error(ErrorCode::already_exists, "the monitor is already open"));
    auto guid = _impl->session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto listening = _impl->session.listen();
    SIHD_UNEXPECTED_RETURN(listening);
    _impl->guid = *guid;
    _impl->listening = true;
    return {};
}

Result<std::optional<std::vector<Monitor::Event>>> Monitor::_receive(Duration timeout)
{
    if (!_impl->listening)
        return std::unexpected(Error(ErrorCode::not_initialized, "the monitor is not open"));
    const std::optional<wlanapi::Notification> notification = _impl->session.next(timeout);
    if (!notification.has_value())
        return std::nullopt;
    std::vector<Event> events;
    const std::optional<Event> event = notification->interface_guid == _impl->guid ? to_event(*notification)
                                                                                   : std::nullopt;
    if (event.has_value())
        events.emplace_back(*event);
    return events;
}

} // namespace sihd::wifi
