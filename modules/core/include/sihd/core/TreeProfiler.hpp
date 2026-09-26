#ifndef __SIHD_CORE_TREEPROFILER_HPP__
#define __SIHD_CORE_TREEPROFILER_HPP__

#include <atomic>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <sihd/core/Channel.hpp>
#include <sihd/core/ChannelMatch.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/IObserverWatcher.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

namespace sihd::core
{

// a channel notification or service operation, traced strace-like: an enter event
// is emitted when it starts and an exit event carries the measured duration
struct TreeProfilerEvent
{
        enum Kind
        {
            enter,
            exit
        };

        sihd::util::Timestamp timestamp {0};
        size_t id = 0;
        pthread_t thread_id = 0;
        std::string source;
        std::string what;
        Kind kind = exit;
        sihd::util::time::UnixTime duration = 0;
        bool success = true;
};

// strace-like profiler of channel notifications and service operations; the
// observed objects must outlive it, or be detached first with reset()
class TreeProfiler: public sihd::util::IHandler<sihd::util::ServiceController *>,
                    public sihd::util::Observable<TreeProfilerEvent>
{
    private:
        using Operation = sihd::util::AService::Operation;

        struct ChannelEntry: public sihd::util::IObserverWatcher<Channel>
        {
                struct ObsStat
                {
                        sihd::util::IHandler<Channel *> *obs = nullptr;
                        std::string label;
                        sihd::util::Stat<sihd::util::time::UnixTime> stat;
                        sihd::util::time::UnixTime last = 0;
                };

                Channel *channel = nullptr;
                TreeProfiler *profiler = nullptr;
                std::vector<ObsStat> stats;
                sihd::util::Stat<sihd::util::time::UnixTime> notify_stat;
                // Channel::write rejects re-entrant writes: one notification in flight
                sihd::util::IHandler<Channel *> *pending_obs = nullptr;
                sihd::util::Timestamp pending_begin {0};
                bool write_in_flight = false;
                sihd::util::Timestamp write_begin {0};
                bool write_recorded = false;
                std::vector<std::string> write_detail;

                void before_notification(Channel *sender) override;
                void after_notification(Channel *sender) override;
                void before_observer(sihd::util::IHandler<Channel *> *obs, Channel *sender) override;
                void after_observer(sihd::util::IHandler<Channel *> *obs, Channel *sender) override;

                ObsStat *_find_stat(sihd::util::IHandler<Channel *> *obs);
                void _prune_stats();
        };

        struct ServiceEntry
        {
                sihd::util::AService *service = nullptr;
                sihd::util::ServiceController *ctrl = nullptr;
                sihd::util::Timestamp op_begin {0};
                Operation pending_op = sihd::util::AService::Error;
                bool op_pending = false;
                bool op_enter_recorded = false;
                std::map<Operation, sihd::util::Stat<sihd::util::time::UnixTime>> op_stats;
        };

    public:
        using Event = TreeProfilerEvent;
        using ChannelCondition = std::function<bool(Channel *)>;
        using ServiceCondition = std::function<bool(sihd::util::ServiceController *)>;

        struct ReportOpts
        {
                size_t max_recursion = 0;
                bool description = false;
        };

        TreeProfiler();
        virtual ~TreeProfiler();

        TreeProfiler(const TreeProfiler &) = delete;
        TreeProfiler & operator=(const TreeProfiler &) = delete;

        // decorates every channel and service found recursively from root;
        // children created during lifecycle operations are picked up as services
        // change state
        bool observe(sihd::util::Node *root, size_t max_recursion = 0);
        bool observe(Channel *channel);
        bool observe(sihd::util::AService *service);

        void reset();

        void clear();

        void set_trace(bool active);

        // set before observe: wrapped handlers keep the clock they were created with
        void set_clock(sihd::util::IClock *clock);

        // keeps at most max_events most recent events in the polled history, 0 keeps everything
        void set_max_events(size_t max_events);

        // events are batched on an internal thread; past queue_max (0 = unbounded) they
        // drop and enter/exit pairs break, past warning_threshold (0 disables) one warning
        // then one error is logged per source until the queue drains
        void set_queue_max(size_t queue_max);
        void set_queue_warning(size_t warning_threshold);

        bool flush(sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(1))) const;

        size_t dropped_events() const;

        bool add_observer(sihd::util::IHandler<Event *> *obs, bool add_to_front = false);

        // capture window: open unless start conditions exist; they latch on their object's
        // notifications and open the capture when all are satisfied (also once at attach),
        // stop conditions latch during the capture and close it when all are satisfied; the
        // notification that opened or closed the window is captured; paths resolve against
        // the observed roots, now or when the object shows up
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

        // duration of the capture session in progress, frozen at the duration of
        // the last one when the window is closed
        sihd::util::Duration session_duration() const;

        // a closed window rearms the start conditions when set, opening a new
        // capture when they are satisfied again
        void set_rearm(bool rearm);

        std::string report_str() const;
        // empty when called from inside a service op; concurrent calls wait for
        // the running report
        std::string report_str(const ReportOpts & opts) const;
        void log_report() const;
        void log_report(const ReportOpts & opts) const;

        std::string events_str() const;
        std::vector<Event> events() const;

        size_t channels_count() const;
        size_t services_count() const;

        void handle(sihd::util::ServiceController *ctrl) override;

    private:
        struct DispatchQueue
        {
                // one waitable serves the has-items and drained waits: notify_all on both sides
                sihd::util::Waitable waitable;
                std::deque<Event> items;
                size_t pending = 0;
        };

        // the profiler lock doubles as the ops gate: a report holds the walk flag below
        // to stop the service notification chain in op_start, before an op body can
        // create or remove children; nested ops on a thread already running one belong
        // to the awaited cascade
        // a notification takes the profiler lock while holding its channel or observable
        // lock: their apis are called without the profiler lock held
        mutable sihd::util::WaitableRecursive _waitable;
        mutable bool _walk_in_progress = false;
        mutable size_t _ops_in_flight = 0;
        mutable std::map<std::thread::id, size_t> _ops_depth;

        struct CaptureCondition
        {
                std::variant<ChannelMatch, ChannelCondition, ServiceCondition> matcher;
                std::string path;
                Channel *channel = nullptr;
                sihd::util::AService *service = nullptr;
                sihd::util::ServiceController *ctrl = nullptr;
                bool is_start = true;
                bool satisfied = false;
                bool attached = false;
                bool dropped = false;
        };

        using ConditionList = std::list<std::shared_ptr<CaptureCondition>>;

        // how a notification met the capture window
        struct WindowCheck
        {
                bool was_open = false;
                bool opened = false;

                bool captured() const;
        };

        void _observe_node(sihd::util::Node *node,
                           size_t current_recursion,
                           size_t max_recursion,
                           std::set<sihd::util::Node *> & visited);
        bool _observe_channel(Channel *channel);
        bool _observe_service(sihd::util::AService *service);
        void _add_root(sihd::util::Named *root);

        void _notify_begin(ChannelEntry *entry);
        void _notify_end(ChannelEntry *entry);
        void _observer_begin(ChannelEntry *entry, sihd::util::IHandler<Channel *> *obs);
        void _observer_end(ChannelEntry *entry, sihd::util::IHandler<Channel *> *obs);

        // runs under the profiler lock, returns the trace line to log after it is released
        std::string _handle_op_enter(ServiceEntry *entry,
                                     sihd::util::ServiceController *ctrl,
                                     Operation started_op,
                                     const std::string & name);
        std::string _handle_op_exit(ServiceEntry *entry, sihd::util::ServiceController *ctrl, const std::string & name);
        void _push_ops_depth();
        void _pop_ops_depth();
        std::optional<std::string> _handle_op(ServiceEntry *entry,
                                              sihd::util::ServiceController *ctrl,
                                              const std::optional<Operation> & started_op,
                                              const std::string & name);

        Event _push_event(sihd::util::Timestamp timestamp,
                          const std::string & source,
                          const std::string & what,
                          Event::Kind kind,
                          sihd::util::time::UnixTime duration,
                          bool success);

        void _emit_enter(sihd::util::Timestamp timestamp, const std::string & source, const std::string & what);
        void _emit_exit(sihd::util::Timestamp timestamp,
                        const std::string & source,
                        const std::string & what,
                        sihd::util::time::UnixTime duration,
                        bool success);

        void _dispatch(Event && event);
        void _ensure_dispatcher();
        void _stop_dispatcher();
        void _dispatch_loop();

        bool _when(bool is_start, Channel *channel, ChannelMatch match);
        bool _when(bool is_start, Channel *channel, ChannelCondition cond);
        bool _when(bool is_start, sihd::util::AService *service, ServiceCondition cond);
        bool _when(bool is_start, const std::string & path, ChannelMatch match);
        bool _when(bool is_start, const std::string & path, ChannelCondition cond);
        bool _when(bool is_start, const std::string & path, ServiceCondition cond);
        bool _add_condition(bool is_start, CaptureCondition && entry);
        void _resolve_pending_conditions();
        void _resolve_condition(const std::shared_ptr<CaptureCondition> & cond);
        bool _resolve_channel_condition(const std::shared_ptr<CaptureCondition> & cond);
        bool _resolve_service_condition(const std::shared_ptr<CaptureCondition> & cond);
        sihd::util::Named *_find_from_roots(const std::string & path);
        WindowCheck _evaluate_window_conditions(const void *target, bool by_ctrl);
        WindowCheck _evaluate_window_conditions(Channel *channel);
        WindowCheck _evaluate_window_conditions(sihd::util::ServiceController *ctrl);
        void _evaluate_conditions(const void *target, bool by_ctrl);
        void _evaluate_condition(const std::shared_ptr<CaptureCondition> & cond);
        bool _all_satisfied(const ConditionList & conditions) const;
        void _update_window();
        void _open_window();
        void _close_window();

        void _report_named(const sihd::util::Named *named,
                           const sihd::util::Node *parent,
                           const std::string & key,
                           std::string & s,
                           const ReportOpts & opts,
                           size_t depth,
                           std::set<const sihd::util::Named *> & visited) const;

        ServiceEntry *_find_service_entry(sihd::util::ServiceController *ctrl);
        const ServiceEntry *_cfind_service_entry(const sihd::util::AService *service) const;

        void _collect_live(const sihd::util::Named *named,
                           std::set<const sihd::util::Named *> & nameds,
                           std::set<const sihd::util::AService *> & services) const;

        sihd::util::Timestamp _now() const;

        std::atomic<bool> _trace {false};
        std::atomic<size_t> _max_events {0};
        size_t _event_counter = 0;
        std::atomic<size_t> _queue_max {4096};
        std::atomic<size_t> _queue_warning {1024};
        std::atomic<bool> _stop_dispatch {false};
        std::atomic<bool> _dispatcher_running {false};
        std::atomic<size_t> _dropped_events_total {0};
        // sources already logged at the current queue pressure, guarded by the
        // dispatch queue waitable
        std::set<std::string> _warned_sources;
        std::set<std::string> _errored_sources;
        mutable DispatchQueue _dispatch_queue;
        sihd::util::Worker _worker;
        sihd::util::IClock *_clock_ptr = nullptr;
        sihd::util::SteadyClock _clock;
        std::atomic<bool> _capturing {true};
        std::atomic<bool> _rearm {false};
        bool _window_done = false;
        size_t _window_opens = 0;
        sihd::util::Timestamp _session_begin {0};
        sihd::util::Duration _last_session_duration {0};
        std::vector<sihd::util::Named *> _roots;
        std::deque<Event> _events;
        ConditionList _start_conditions;
        ConditionList _stop_conditions;
        std::map<Channel *, std::unique_ptr<ChannelEntry>> _channels;
        std::map<sihd::util::ServiceController *, std::unique_ptr<ServiceEntry>> _services;
};

} // namespace sihd::core

#endif
