#ifndef __SIHD_CORE_TREEPROFILER_HPP__
#define __SIHD_CORE_TREEPROFILER_HPP__

#include <atomic>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <sihd/core/Channel.hpp>
#include <sihd/core/ChannelMatch.hpp>
#include <sihd/core/TreeMonitor.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/time.hpp>

namespace sihd::core
{

// traced strace-like: an enter event at start, an exit event carrying the measured duration
struct TreeProfilerEvent
{
        enum Kind
        {
            enter,
            exit
        };

        sihd::util::Timestamp timestamp {0};
        size_t id = 0;
        std::thread::id thread_id;
        std::string source;
        std::string what;
        Kind kind = exit;
        sihd::util::time::UnixTime duration = 0;
        bool success = true;
};

// subscribes to a monitor that outlives it
class TreeProfiler: public sihd::util::IHandler<TreeMonitorEvent *>,
                    public sihd::util::Observable<TreeProfilerEvent>
{
    private:
        using Operation = sihd::util::AService::Operation;

        struct ObserverStat
        {
                TreeMonitorEvent::Observer observer;
                sihd::util::Stat<sihd::util::time::UnixTime> stat;
        };

        struct ChannelStats
        {
                std::vector<ObserverStat> observers;
                sihd::util::Stat<sihd::util::time::UnixTime> notify_stat;
                bool write_in_flight = false;
                bool write_recorded = false;
        };

        struct ServiceStats
        {
                sihd::util::Timestamp op_begin {0};
                Operation pending_op = sihd::util::AService::Failure;
                bool op_pending = false;
                bool op_enter_recorded = false;
                std::map<Operation, sihd::util::Stat<sihd::util::time::UnixTime>> op_stats;
        };

    public:
        using Event = TreeProfilerEvent;
        using ChannelCondition = std::function<bool(const sihd::util::IArray &)>;
        using ServiceCondition = std::function<bool(sihd::util::ServiceController::State)>;

        struct ReportOpts
        {
                size_t max_recursion = 0;
                bool description = false;
        };

        explicit TreeProfiler(TreeMonitor & monitor);
        virtual ~TreeProfiler();

        TreeProfiler(const TreeProfiler &) = delete;
        TreeProfiler & operator=(const TreeProfiler &) = delete;

        void reset();

        void clear();

        void set_trace(bool active);

        // keeps at most max_events most recent events in the polled history, 0 keeps everything
        void set_max_events(size_t max_events);

        // start conditions latch and open the window when all fire (also once at attach), stop ones close it
        bool start_when(Channel *channel, ChannelMatch match);
        bool start_when(Channel *channel, ChannelCondition cond);
        bool start_when(sihd::util::AService *service, ServiceCondition cond);
        bool start_when(const std::string & path, ChannelMatch match);
        bool start_when(const std::string & path, ChannelCondition cond);
        bool start_when(const std::string & path, ServiceCondition cond);
        bool stop_when(Channel *channel, ChannelMatch match);
        bool stop_when(Channel *channel, ChannelCondition cond);
        bool stop_when(sihd::util::AService *service, ServiceCondition cond);
        bool stop_when(const std::string & path, ChannelMatch match);
        bool stop_when(const std::string & path, ChannelCondition cond);
        bool stop_when(const std::string & path, ServiceCondition cond);

        static ServiceCondition running();
        static ServiceCondition stopped();

        void start_capture();
        void stop_capture();
        bool capturing() const;

        // frozen at the last session duration once the window is closed
        sihd::util::Duration session_duration() const;

        // a closed window rearms the start conditions when set
        void set_rearm(bool rearm);

        std::string report_str() const;
        // empty when called from inside a service op; concurrent calls wait for the running report
        std::string report_str(const ReportOpts & opts) const;
        void log_report() const;
        void log_report(const ReportOpts & opts) const;

        std::string events_str() const;
        std::vector<Event> events() const;

        void handle(TreeMonitorEvent *event) override;

    private:
        struct CaptureCondition
        {
                std::variant<ChannelMatch, ChannelCondition, ServiceCondition> matcher;
                std::string path {};
                Channel *channel = nullptr;
                sihd::util::AService *service = nullptr;
                bool is_start = true;
                bool satisfied = false;
                bool attached = false;
                bool dropped = false;
        };

        using ConditionList = std::list<std::shared_ptr<CaptureCondition>>;

        struct WindowCheck
        {
                bool was_open = false;
                bool opened = false;

                bool captured() const;
        };

        void _flush() const;

        void _handle_write_begin(const TreeMonitorEvent & event, std::vector<Event> & emitted);
        void _handle_write_end(const TreeMonitorEvent & event, std::vector<Event> & emitted);
        void _handle_op_begin(const TreeMonitorEvent & event, std::vector<Event> & emitted);
        void _handle_op_end(const TreeMonitorEvent & event, std::vector<Event> & emitted);
        void _handle_detached(const TreeMonitorEvent & event);

        bool _when(bool is_start, CaptureCondition && entry);
        void _resolve_pending_conditions(sihd::util::Timestamp now, const TreeMonitorEvent *event);
        void _resolve_condition(const std::shared_ptr<CaptureCondition> & cond,
                                sihd::util::Timestamp now,
                                const TreeMonitorEvent *event);
        bool _verify_match(const CaptureCondition & cond, const Channel *channel, const TreeMonitorEvent *event) const;

        WindowCheck _evaluate_window_conditions(const TreeMonitorEvent & event);
        bool _matches_event(const CaptureCondition & cond, const TreeMonitorEvent & event) const;
        bool _matches_live(const CaptureCondition & cond) const;
        void _settle_condition(CaptureCondition & cond, bool result, sihd::util::Timestamp now);
        bool _condition_active(const CaptureCondition & cond) const;
        bool _all_satisfied(const ConditionList & conditions) const;
        void _update_window(sihd::util::Timestamp now);
        void _open_window(sihd::util::Timestamp now);
        void _close_window(sihd::util::Timestamp now);

        Event _push_event(const TreeMonitorEvent & source_event,
                          sihd::util::Timestamp timestamp,
                          const std::string & what,
                          Event::Kind kind,
                          sihd::util::time::UnixTime duration,
                          bool success);

        void _report_named(sihd::util::Named *named,
                           const sihd::util::Node *parent,
                           const std::string & key,
                           size_t depth,
                           const ReportOpts & opts,
                           std::string & s) const;

        TreeMonitor & _monitor;
        // taken after the monitor tree lock (walk), never around a monitor call
        mutable std::recursive_mutex _mutex;
        std::atomic<bool> _trace {false};
        std::atomic<size_t> _max_events {0};
        size_t _event_counter = 0;
        std::atomic<bool> _capturing {true};
        std::atomic<bool> _rearm {false};
        bool _window_done = false;
        size_t _window_opens = 0;
        sihd::util::Timestamp _session_begin {0};
        sihd::util::Duration _last_session_duration {0};
        std::deque<Event> _events;
        ConditionList _start_conditions;
        ConditionList _stop_conditions;
        std::map<const Channel *, ChannelStats> _channels;
        std::map<const sihd::util::AService *, ServiceStats> _services;
};

} // namespace sihd::core

#endif
