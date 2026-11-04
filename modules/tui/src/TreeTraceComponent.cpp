#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>

#include <fmt/format.h>

#include <sihd/tui/TableComponent.hpp>
#include <sihd/tui/TreeTraceComponent.hpp>
#include <sihd/tui/channel_view.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/ServiceController.hpp>

namespace sihd::tui
{

using namespace ftxui;
using namespace sihd::util;
using namespace sihd::core;

namespace
{

const char *kind_str(TreeMonitorEvent::Kind kind)
{
    switch (kind)
    {
        case TreeMonitorEvent::channel_attached:
        case TreeMonitorEvent::service_attached:
            return "attach";
        case TreeMonitorEvent::channel_detached:
        case TreeMonitorEvent::service_detached:
            return "detach";
        case TreeMonitorEvent::write_begin:
            return "write";
        case TreeMonitorEvent::write_end:
            return "written";
        case TreeMonitorEvent::op_begin:
            return "op";
        case TreeMonitorEvent::op_end:
            return "op end";
    }
    return "";
}

std::string detail_str(const TreeMonitorEvent & event)
{
    switch (event.kind)
    {
        case TreeMonitorEvent::channel_attached:
        case TreeMonitorEvent::write_begin:
            return event.value != nullptr ? array_str(*event.value) : "";
        case TreeMonitorEvent::service_attached:
        case TreeMonitorEvent::op_begin:
            return ServiceController::state_str(*event.state);
        case TreeMonitorEvent::write_end:
            return Duration(*event.duration).str();
        case TreeMonitorEvent::op_end:
            return fmt::format("{} {}{}",
                               ServiceController::state_str(*event.state),
                               Duration(*event.duration).str(),
                               *event.success ? "" : " (error)");
        default:
            return "";
    }
    return "";
}

class TraceFeed: public IHandler<TreeMonitorEvent *>
{
    public:
        TraceFeed(TreeMonitor & monitor, Refresher & refresher, size_t max_events):
            _monitor(monitor),
            _refresher(refresher),
            _max_events(std::max<size_t>(max_events, 1))
        {
            _monitor.add_observer(this);
        }

        ~TraceFeed() override { _monitor.remove_observer(this); }

        void handle(TreeMonitorEvent *event) override
        {
            {
                std::lock_guard lock(_mutex);
                _count += 1;
                _rows.push_back({std::to_string(_count), kind_str(event->kind), *event->path, detail_str(*event)});
                if (_rows.size() > _max_events)
                    _rows.pop_front();
            }
            _refresher.request();
        }

        TableRows rows()
        {
            std::lock_guard lock(_mutex);
            return TableRows(_rows.begin(), _rows.end());
        }

    private:
        TreeMonitor & _monitor;
        Refresher & _refresher;
        size_t _max_events;
        std::mutex _mutex;
        size_t _count = 0;
        std::deque<std::vector<std::string>> _rows;
};

} // namespace

Component TreeTraceComponent(TreeMonitor & monitor, Refresher & refresher, size_t max_events)
{
    auto feed = std::make_shared<TraceFeed>(monitor, refresher, max_events);
    TableOptions options;
    options.columns = {"#", "event", "path", "detail"};
    options.rows = [feed] {
        return feed->rows();
    };
    options.sort_column = 0;
    options.sort_descending = true;
    return TableComponent(std::move(options));
}

} // namespace sihd::tui
