#ifndef __SIHD_CORE_TREEMONITOR_HPP__
#define __SIHD_CORE_TREEMONITOR_HPP__

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <sihd/core/Channel.hpp>
#include <sihd/util/AService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/IObserverWatcher.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/time.hpp>

namespace sihd::core
{

struct TreeMonitorEvent
{
        enum Kind
        {
            channel_attached,
            service_attached,
            channel_detached,
            service_detached,
            write_begin,
            write_end,
            op_begin,
            op_end
        };

        struct Observer
        {
                const sihd::util::IHandler<Channel *> *handler = nullptr;
                std::shared_ptr<const std::string> label;
        };

        struct ObserverTime
        {
                Observer observer;
                sihd::util::time::UnixTime duration = 0;
        };

        Kind kind = write_end;
        sihd::util::Timestamp timestamp {0};
        std::thread::id thread_id;
        std::shared_ptr<const std::string> path;
        // identities only: the object may be gone at delivery, never dereferenced outside TreeMonitor::walk
        const Channel *channel = nullptr;
        const sihd::util::AService *service = nullptr;
        std::shared_ptr<const sihd::util::IArray> value;
        std::optional<sihd::util::ServiceController::State> state;
        std::optional<sihd::util::AService::Operation> operation;
        std::optional<sihd::util::time::UnixTime> duration;
        std::optional<bool> success;
        std::optional<std::vector<ObserverTime>> observers;
};

// observed objects must outlive it, or be detached first with reset()
class TreeMonitor: public sihd::util::IHandler<sihd::util::ServiceController *>,
                   public sihd::util::Observable<TreeMonitorEvent>
{
    private:
        using Operation = sihd::util::AService::Operation;

        struct ChannelEntry: public sihd::util::IObserverWatcher<Channel>
        {
                Channel *channel = nullptr;
                TreeMonitor *monitor = nullptr;
                std::shared_ptr<const std::string> path;
                // set once the watcher slot is taken: a prune never drops an entry still being claimed
                bool watching = false;
                // Channel::write rejects re-entrant writes: one notification in flight
                bool write_in_flight = false;
                sihd::util::Timestamp write_begin {0};
                sihd::util::Timestamp observer_begin {0};
                std::vector<TreeMonitorEvent::ObserverTime> observers;
                std::vector<TreeMonitorEvent::Observer> labels;

                void before_notification(Channel *sender) override;
                void after_notification(Channel *sender) override;
                void before_observer(sihd::util::IHandler<Channel *> *obs, Channel *sender) override;
                void after_observer(sihd::util::IHandler<Channel *> *obs, Channel *sender) override;

                const TreeMonitorEvent::Observer & _observer(sihd::util::IHandler<Channel *> *obs);
        };

        struct ServiceEntry
        {
                sihd::util::AService *service = nullptr;
                sihd::util::ServiceController *ctrl = nullptr;
                std::shared_ptr<const std::string> path;
                sihd::util::Timestamp op_begin {0};
                Operation pending_op = sihd::util::AService::Failure;
                bool op_pending = false;
        };

        struct Root
        {
                sihd::util::Named *named = nullptr;
                // empty: the root alone, its children are not observed
                std::optional<size_t> max_recursion;
        };

    public:
        using Event = TreeMonitorEvent;
        using Visitor = std::function<
            void(sihd::util::Named *named, const sihd::util::Node *parent, const std::string & key, size_t depth)>;

        TreeMonitor();
        virtual ~TreeMonitor();

        TreeMonitor(const TreeMonitor &) = delete;
        TreeMonitor & operator=(const TreeMonitor &) = delete;

        bool observe(sihd::util::Node *root, size_t max_recursion = 0);
        bool observe(Channel *channel);
        bool observe(sihd::util::AService *service);

        bool observes(const Channel *channel) const;
        bool observes(const sihd::util::AService *service) const;

        void reset();

        void set_clock(sihd::util::IClock *clock);
        sihd::util::Timestamp now() const;

        // past queue_max (0 = unbounded) events drop and begin/end pairs break; warnings past warning_threshold
        void set_queue_max(size_t queue_max);
        void set_queue_warning(size_t warning_threshold);

        // false at once from the dispatch thread: it cannot wait for itself
        bool flush(sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(1))) const;

        size_t dropped_events() const;

        bool add_observer(sihd::util::IHandler<Event *> *obs, bool add_to_front = false);
        void remove_observer(sihd::util::IHandler<Event *> *obs);

        sihd::util::Named *find(const std::string & path) const;

        // false inside an op or when in-flight ops outlast the timeout
        bool walk(const Visitor & visit,
                  size_t max_recursion = 0,
                  sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(1))) const;

        size_t channels_count() const;
        size_t services_count() const;

        void handle(sihd::util::ServiceController *ctrl) override;

    private:
        void _observe_node(sihd::util::Node *node,
                           size_t current_recursion,
                           size_t max_recursion,
                           std::set<sihd::util::Node *> & visited);
        bool _observe_channel(Channel *channel);
        bool _observe_service(sihd::util::AService *service);
        void _detach(bool notify);
        void _add_root(sihd::util::Named *root, std::optional<size_t> max_recursion);
        std::optional<size_t> _rescan_recursion(const sihd::util::Node *node) const;
        void _rescan(sihd::util::Node *node);
        void _prune(const sihd::util::Node *node);

        void _notify_begin(ChannelEntry *entry);
        void _notify_end(ChannelEntry *entry);

        bool _handle_op(ServiceEntry *entry,
                        sihd::util::ServiceController *ctrl,
                        const std::optional<Operation> & started_op);
        void _handle_op_enter(ServiceEntry *entry, sihd::util::ServiceController *ctrl, Operation started_op);
        void _handle_op_exit(ServiceEntry *entry, sihd::util::ServiceController *ctrl);
        void _push_ops_depth();
        void _pop_ops_depth();

        Event _make_event(Event::Kind kind,
                          const std::shared_ptr<const std::string> & path,
                          sihd::util::Timestamp timestamp) const;
        bool _has_subscribers() const;

        void _dispatch(Event && event);
        void _ensure_dispatcher();
        void _stop_dispatcher();
        void _dispatch_loop();

        void _walk_named(sihd::util::Named *named,
                         const sihd::util::Node *parent,
                         const std::string & key,
                         const Visitor & visit,
                         size_t max_recursion,
                         size_t depth,
                         std::set<const sihd::util::Named *> & visited) const;
        void _collect_live(const sihd::util::Named *named,
                           std::set<const sihd::util::Named *> & nameds,
                           std::set<const sihd::util::AService *> & services) const;
        sihd::util::Named *_find_from_roots(const std::string & path) const;

        // the tree lock doubles as the ops gate: a walk holds the walk flag to block op_start; taken last
        mutable sihd::util::WaitableRecursive _waitable;
        mutable bool _walk_in_progress = false;
        size_t _ops_in_flight = 0;
        std::map<std::thread::id, size_t> _ops_depth;
        std::set<const Channel *> _refused_channels;

        std::atomic<size_t> _queue_max {4096};
        std::atomic<size_t> _queue_warning {1024};
        std::atomic<bool> _dispatcher_running {false};
        std::atomic<std::thread::id> _dispatcher_thread;
        std::atomic<size_t> _dropped_events_total {0};
        sihd::util::SafeQueue<Event> _queue;
        // guarded by _pending_waitable
        mutable sihd::util::Waitable _pending_waitable;
        size_t _pending = 0;
        std::set<std::string> _warned_sources;
        std::set<std::string> _errored_sources;
        std::recursive_mutex _dispatch_mutex;
        sihd::util::Worker _worker;
        sihd::util::IClock *_clock_ptr = nullptr;
        sihd::util::SteadyClock _clock;
        std::vector<Root> _roots;
        std::map<Channel *, std::unique_ptr<ChannelEntry>> _channels;
        std::map<sihd::util::ServiceController *, std::unique_ptr<ServiceEntry>> _services;
};

} // namespace sihd::core

#endif
