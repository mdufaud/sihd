#include <sihd/net/NetMonitor.hpp>

namespace sihd::net
{

std::string NetMonitorEvent::type_str() const
{
    switch (type)
    {
        case NetMonitorEventType::link:
            return "link";
        case NetMonitorEventType::address:
            return "address";
        case NetMonitorEventType::route:
            return "route";
        case NetMonitorEventType::overflow:
            return "overflow";
        default:
            return "unknown";
    }
}

NetMonitor::NetMonitor(sihd::util::Duration run_timeout): NetMonitor()
{
    this->set_run_timeout(run_timeout);
}

void NetMonitor::set_run_timeout(sihd::util::Duration timeout)
{
    _run_timeout = timeout;
}

const std::vector<NetMonitorEvent> & NetMonitor::events() const
{
    return _events;
}

} // namespace sihd::net
