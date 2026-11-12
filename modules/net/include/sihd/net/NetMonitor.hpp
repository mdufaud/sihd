#ifndef __SIHD_NET_NETMONITOR_HPP__
#define __SIHD_NET_NETMONITOR_HPP__

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

enum class NetMonitorEventType
{
    unknown,
    link,
    address,
    route,
    // notifications were dropped: anything may have changed
    overflow,
};

struct NetMonitorEvent
{
        NetMonitorEventType type = NetMonitorEventType::unknown;
        bool removed = false;
        std::string interface_name;
        // gained or lost by an address event
        IpAddr address;

        std::string type_str() const;
};

class NetMonitor: public sihd::util::Observable<NetMonitor>,
                  public sihd::util::IRunnable
{
    public:
        NetMonitor();
        explicit NetMonitor(sihd::util::Duration run_timeout);
        virtual ~NetMonitor();

        sihd::util::Result<void> open();
        bool is_open() const;

        // a negative timeout waits for a change
        void set_run_timeout(sihd::util::Duration timeout);

        // false until opened
        bool run() override;

        const std::vector<NetMonitorEvent> & events() const;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
        sihd::util::Duration _run_timeout = std::chrono::milliseconds(100);
        std::vector<NetMonitorEvent> _events;
};

} // namespace sihd::net

#endif
