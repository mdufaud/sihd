#include <winsock2.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

#include <sihd/net/NetMonitor.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include "adapters.hpp"

using namespace sihd::util;

namespace sihd::net
{

namespace
{

constexpr size_t max_pending_events = 4096;

Result<void> check_subscribed(unsigned long code, std::string_view notifications)
{
    if (code != NO_ERROR)
    {
        return std::unexpected(Error(ErrorCode::io_error,
                                     "{} notifications: {}",
                                     notifications,
                                     sihd::sys::os::error_str(static_cast<int>(code))));
    }
    return {};
}

} // namespace

// the system calls the notification callbacks from its thread pool: they queue under the waitable, run() drains
struct NetMonitor::Impl
{
        Waitable waitable;
        // guarded by waitable
        std::vector<NetMonitorEvent> pending;
        std::map<uint64_t, std::string> aliases;
        HANDLE interface_notification = nullptr;
        HANDLE address_notification = nullptr;
        HANDLE route_notification = nullptr;

        ~Impl();

        Result<void> open();

        std::string interface_name(const NET_LUID & luid);
        void push(NetMonitorEventType type, MIB_NOTIFICATION_TYPE notification, const NET_LUID & luid, IpAddr address);

        static VOID WINAPI on_interface(PVOID context, PMIB_IPINTERFACE_ROW row, MIB_NOTIFICATION_TYPE notification);
        static VOID WINAPI on_address(PVOID context, PMIB_UNICASTIPADDRESS_ROW row, MIB_NOTIFICATION_TYPE notification);
        static VOID WINAPI on_route(PVOID context, PMIB_IPFORWARD_ROW2 row, MIB_NOTIFICATION_TYPE notification);
};

// CancelMibChangeNotify2 waits for the running callbacks, so the queue outlives them
NetMonitor::Impl::~Impl()
{
    for (HANDLE notification : {interface_notification, address_notification, route_notification})
    {
        if (notification != nullptr)
            (void)CancelMibChangeNotify2(notification);
    }
}

// a removed interface no longer resolves its luid: the name it was last seen with stands in
std::string NetMonitor::Impl::interface_name(const NET_LUID & luid)
{
    std::string alias = adapters::alias_of(luid);
    if (alias.empty())
    {
        const auto known = aliases.find(luid.Value);
        return known == aliases.end() ? "" : known->second;
    }
    aliases[luid.Value] = alias;
    return alias;
}

void NetMonitor::Impl::push(NetMonitorEventType type,
                            MIB_NOTIFICATION_TYPE notification,
                            const NET_LUID & luid,
                            IpAddr address)
{
    NetMonitorEvent event;
    event.type = type;
    event.removed = notification == MibDeleteInstance;
    event.address = std::move(address);

    {
        auto lock = waitable.guard();
        event.interface_name = this->interface_name(luid);
        if (pending.size() < max_pending_events)
            pending.push_back(std::move(event));
        else if (pending.back().type != NetMonitorEventType::overflow)
        {
            NetMonitorEvent overflow;
            overflow.type = NetMonitorEventType::overflow;
            pending.push_back(std::move(overflow));
        }
    }
    waitable.notify_all();
}

VOID WINAPI NetMonitor::Impl::on_interface(PVOID context, PMIB_IPINTERFACE_ROW row, MIB_NOTIFICATION_TYPE notification)
{
    if (row != nullptr)
        static_cast<Impl *>(context)->push(NetMonitorEventType::link, notification, row->InterfaceLuid, {});
}

VOID WINAPI NetMonitor::Impl::on_address(PVOID context,
                                         PMIB_UNICASTIPADDRESS_ROW row,
                                         MIB_NOTIFICATION_TYPE notification)
{
    if (row != nullptr)
        static_cast<Impl *>(context)->push(NetMonitorEventType::address,
                                           notification,
                                           row->InterfaceLuid,
                                           adapters::to_ip(row->Address));
}

VOID WINAPI NetMonitor::Impl::on_route(PVOID context, PMIB_IPFORWARD_ROW2 row, MIB_NOTIFICATION_TYPE notification)
{
    if (row != nullptr)
        static_cast<Impl *>(context)->push(NetMonitorEventType::route, notification, row->InterfaceLuid, {});
}

// the callbacks reach the impl by address: it never moves once subscribed
Result<void> NetMonitor::Impl::open()
{
    auto adapter_list = adapters::list(GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                                       | GAA_FLAG_SKIP_DNS_SERVER);
    SIHD_UNEXPECTED_RETURN(adapter_list);
    for (const IP_ADAPTER_ADDRESSES *adapter : adapters::named(*adapter_list))
        aliases[adapter->Luid.Value] = adapters::friendly_name(*adapter);

    auto interfaces = check_subscribed(
        NotifyIpInterfaceChange(AF_UNSPEC, &on_interface, this, FALSE, &interface_notification),
        "interface");
    SIHD_UNEXPECTED_RETURN(interfaces);
    auto addresses = check_subscribed(
        NotifyUnicastIpAddressChange(AF_UNSPEC, &on_address, this, FALSE, &address_notification),
        "address");
    SIHD_UNEXPECTED_RETURN(addresses);
    auto routes = check_subscribed(NotifyRouteChange2(AF_UNSPEC, &on_route, this, FALSE, &route_notification), "route");
    SIHD_UNEXPECTED_RETURN(routes);
    return {};
}

NetMonitor::NetMonitor() = default;

NetMonitor::~NetMonitor() = default;

Result<void> NetMonitor::open()
{
    if (this->is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "net monitor already open"));
    auto impl = std::make_unique<Impl>();
    auto opened = impl->open();
    SIHD_UNEXPECTED_RETURN(opened);
    _impl = std::move(impl);
    return {};
}

bool NetMonitor::run()
{
    _events.clear();
    if (!this->is_open())
        return false;
    if (_run_timeout < 0)
    {
        auto lock = _impl->waitable.wait_guard([this] { return !_impl->pending.empty(); });
        _events.swap(_impl->pending);
    }
    else
    {
        auto lock = _impl->waitable.wait_for_guard(_run_timeout, [this] { return !_impl->pending.empty(); });
        _events.swap(_impl->pending);
    }
    if (!_events.empty())
        this->notify_observers(this);
    return true;
}

} // namespace sihd::net
