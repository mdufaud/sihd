#include <algorithm>
#include <optional>
#include <string_view>
#include <typeinfo>

#include <fmt/ranges.h>

#include <sihd/core/TreeProfiler.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

namespace sihd::core
{

using namespace sihd::util;

namespace
{

using Op = sihd::util::AService::Operation;

struct WalkGuard
{
        sihd::util::WaitableRecursive & waitable;
        bool & walk_in_progress;

        ~WalkGuard()
        {
            {
                auto l = waitable.guard();
                walk_in_progress = false;
            }
            waitable.notify_all();
        }
};

std::optional<Op> op_from_state(sihd::util::ServiceController::State state)
{
    switch (state)
    {
        case sihd::util::ServiceController::Configuring:
            return Op::Setup;
        case sihd::util::ServiceController::Initializing:
            return Op::Init;
        case sihd::util::ServiceController::Starting:
            return Op::Start;
        case sihd::util::ServiceController::Stopping:
            return Op::Stop;
        case sihd::util::ServiceController::Resetting:
            return Op::Reset;
        default:
            return std::nullopt;
    }
}

const char *op_str(Op op)
{
    switch (op)
    {
        case Op::Setup:
            return "setup";
        case Op::Init:
            return "init";
        case Op::Start:
            return "start";
        case Op::Stop:
            return "stop";
        case Op::Reset:
            return "reset";
        default:
            return "unknown";
    }
}

std::string duration_str(time::UnixTime nano)
{
    return sihd::util::Duration(nano).str();
}

std::string stat_str(const sihd::util::Stat<time::UnixTime> & stat, std::string_view count_label = "n")
{
    if (stat.samples == 0)
        return fmt::format("{}=0", count_label);
    return fmt::format("{}={} total={} avg={} min={} max={}",
                       count_label,
                       stat.samples,
                       duration_str(stat.sum),
                       duration_str(stat.average()),
                       duration_str(stat.min),
                       duration_str(stat.max));
}

std::string observer_label(sihd::util::IHandler<Channel *> *obs)
{
    const std::string class_name = sihd::util::str::demangle(typeid(*obs).name());
    const sihd::util::Named *named = dynamic_cast<const sihd::util::Named *>(obs);
    if (named != nullptr)
        return fmt::format("{} ({})", named->name(), class_name);
    return class_name;
}

std::string service_name(const sihd::util::AService *service)
{
    const sihd::util::Named *named = dynamic_cast<const sihd::util::Named *>(service);
    if (named != nullptr)
        return named->full_name();
    return sihd::util::str::demangle(typeid(*service).name());
}

} // namespace

SIHD_LOGGER;

TreeProfiler::ChannelEntry::ObsStat *TreeProfiler::ChannelEntry::_find_stat(sihd::util::IHandler<Channel *> *obs)
{
    for (ObsStat & stat : stats)
    {
        if (stat.obs == obs)
            return &stat;
    }
    return nullptr;
}

void TreeProfiler::ChannelEntry::_prune_stats()
{
    // sizes diverge only when an observer came or went
    if (stats.size() == channel->observers_count())
        return;
    std::set<sihd::util::IHandler<Channel *> *> live;
    channel->for_each_observer([&live](sihd::util::IHandler<Channel *> *obs) { live.insert(obs); });
    stats.erase(
        std::remove_if(stats.begin(), stats.end(), [&live](const ObsStat & stat) { return live.count(stat.obs) == 0; }),
        stats.end());
}

void TreeProfiler::ChannelEntry::before_notification([[maybe_unused]] Channel *sender)
{
    profiler->_notify_begin(this);
}

void TreeProfiler::ChannelEntry::after_notification([[maybe_unused]] Channel *sender)
{
    profiler->_notify_end(this);
}

void TreeProfiler::ChannelEntry::before_observer(sihd::util::IHandler<Channel *> *obs, [[maybe_unused]] Channel *sender)
{
    profiler->_observer_begin(this, obs);
}

void TreeProfiler::ChannelEntry::after_observer(sihd::util::IHandler<Channel *> *obs, [[maybe_unused]] Channel *sender)
{
    profiler->_observer_end(this, obs);
}

TreeProfiler::TreeProfiler()
{
    _worker.set_method([this] {
        this->_dispatch_loop();
        return true;
    });
    // without start conditions the session runs from construction
    _session_begin = this->_now();
}

TreeProfiler::~TreeProfiler()
{
    this->reset();
    this->_stop_dispatcher();
}

void TreeProfiler::set_trace(bool active)
{
    _trace = active;
}

void TreeProfiler::set_clock(sihd::util::IClock *clock)
{
    _clock_ptr = clock;
}

void TreeProfiler::set_max_events(size_t max_events)
{
    _max_events = max_events;
}

void TreeProfiler::set_queue_max(size_t queue_max)
{
    _queue_max = queue_max;
}

void TreeProfiler::set_queue_warning(size_t warning_threshold)
{
    _queue_warning = warning_threshold;
}

bool TreeProfiler::add_observer(sihd::util::IHandler<Event *> *obs, bool add_to_front)
{
    const bool ret = Observable<Event>::add_observer(obs, add_to_front);
    if (ret)
        this->_ensure_dispatcher();
    return ret;
}

bool TreeProfiler::flush(sihd::util::Duration timeout) const
{
    return _dispatch_queue.waitable.wait_for(timeout, [this] { return _dispatch_queue.pending == 0; });
}

size_t TreeProfiler::dropped_events() const
{
    return _dropped_events_total;
}

void TreeProfiler::_ensure_dispatcher()
{
    if (_dispatcher_running.exchange(true))
        return;
    _stop_dispatch = false;
    if (_worker.start_worker("sihd.treeprofiler") == false)
        _dispatcher_running = false;
}

void TreeProfiler::_stop_dispatcher()
{
    if (_dispatcher_running.exchange(false) == false)
        return;
    {
        auto l = _dispatch_queue.waitable.guard();
        _stop_dispatch = true;
    }
    _dispatch_queue.waitable.notify_all();
    _worker.stop_worker();
}

void TreeProfiler::_dispatch(Event && event)
{
    if (Observable<Event>::observers_count() == 0)
        return;
    bool warned = false;
    bool errored = false;
    std::string source;
    size_t queue_size = 0;
    {
        auto l = _dispatch_queue.waitable.guard();
        queue_size = _dispatch_queue.items.size();
        const size_t queue_max = _queue_max;
        if (queue_max > 0 && queue_size >= queue_max)
        {
            _dropped_events_total += 1;
            // one error per source until the queue drains again
            errored = _errored_sources.insert(event.source).second;
            if (errored)
                source = event.source;
        }
        else
        {
            const size_t warning_threshold = _queue_warning;
            // one warning per source until the queue drains again
            warned = warning_threshold > 0 && queue_size >= warning_threshold
                     && _warned_sources.insert(event.source).second;
            if (warned)
                source = event.source;
            _dispatch_queue.items.push_back(std::move(event));
            _dispatch_queue.pending += 1;
        }
    }
    if (errored)
    {
        SIHD_LOG_ERROR("TreeProfiler: dispatch queue full, events from '{}' are lost ({} dropped so far)",
                       source,
                       _dropped_events_total.load());
        return;
    }
    if (warned)
        SIHD_LOG_WARN("TreeProfiler: dispatch queue almost full ({}/{}), events from '{}' stack up",
                      queue_size,
                      _queue_max.load(),
                      source);
    _dispatch_queue.waitable.notify_all();
}

void TreeProfiler::_dispatch_loop()
{
    std::vector<Event> batch;
    for (;;)
    {
        batch.clear();
        _dispatch_queue.waitable.wait([this] { return _stop_dispatch || _dispatch_queue.items.empty() == false; });
        {
            auto l = _dispatch_queue.waitable.guard();
            if (_stop_dispatch && _dispatch_queue.items.empty())
                break;
            batch.reserve(_dispatch_queue.items.size());
            for (Event & event : _dispatch_queue.items)
                batch.push_back(std::move(event));
            _dispatch_queue.items.clear();
        }
        for (Event & event : batch)
        {
            // snapshot per event: hooks may add or remove observers, and they run
            // without holding the observable lock so producers never stall on a
            // slow observer
            std::vector<sihd::util::IHandler<Event *> *> observers;
            this->for_each_observer([&observers](sihd::util::IHandler<Event *> *obs) { observers.push_back(obs); });
            for (sihd::util::IHandler<Event *> *obs : observers)
                obs->handle(&event);
        }
        {
            auto l = _dispatch_queue.waitable.guard();
            _dispatch_queue.pending -= batch.size();
            // sources get one warning or error log again at the next pressure
            const size_t warning_threshold = _queue_warning;
            if (warning_threshold == 0 || _dispatch_queue.items.size() < warning_threshold)
                _warned_sources.clear();
            if (_dispatch_queue.items.empty())
                _errored_sources.clear();
        }
        _dispatch_queue.waitable.notify_all();
    }
}

bool TreeProfiler::observe(sihd::util::Node *root, size_t max_recursion)
{
    if (root == nullptr)
        return false;
    sihd::util::AService *service = dynamic_cast<sihd::util::AService *>(root);
    if (service != nullptr)
        this->_observe_service(service);
    std::set<sihd::util::Node *> visited;
    this->_observe_node(root, 0, max_recursion, visited);
    this->_add_root(root);
    this->_resolve_pending_conditions();
    return true;
}

bool TreeProfiler::observe(Channel *channel)
{
    if (channel == nullptr)
        return false;
    const bool ret = this->_observe_channel(channel);
    this->_add_root(channel);
    this->_resolve_pending_conditions();
    return ret;
}

bool TreeProfiler::observe(sihd::util::AService *service)
{
    if (service == nullptr)
        return false;
    const bool ret = this->_observe_service(service);
    // the concrete service is usually a Named: it joins the report roots
    sihd::util::Named *named = dynamic_cast<sihd::util::Named *>(service);
    if (named != nullptr)
        this->_add_root(named);
    this->_resolve_pending_conditions();
    return ret;
}

void TreeProfiler::_add_root(sihd::util::Named *root)
{
    auto l = _waitable.guard();
    if (std::find(_roots.begin(), _roots.end(), root) == _roots.end())
        _roots.push_back(root);
}

void TreeProfiler::reset()
{
    // liveness comes from a walk of the roots: children destroyed by a service
    // state change are not in it anymore, their stale entries are dropped
    // without dereferencing them
    std::set<const sihd::util::Named *> nameds;
    std::set<const sihd::util::AService *> services;
    std::vector<Channel *> channels;
    std::vector<sihd::util::ServiceController *> ctrls;
    {
        auto l = _waitable.guard();
        for (sihd::util::Named *root : _roots)
            this->_collect_live(root, nameds, services);
        for (auto & [channel, entry] : _channels)
        {
            if (nameds.count(channel) > 0)
                channels.push_back(entry->channel);
        }
        for (auto & [ctrl, entry] : _services)
        {
            if (services.count(entry->service) > 0)
                ctrls.push_back(entry->ctrl);
        }
        _roots.clear();
    }
    // lock order: set_watcher takes the channel lock
    for (Channel *channel : channels)
        channel->set_watcher(nullptr);
    for (sihd::util::ServiceController *ctrl : ctrls)
        ctrl->remove_observer(this);
    {
        auto l = _waitable.guard();
        _channels.clear();
        _services.clear();
        _service_entries.clear();
        _events.clear();
        _start_conditions.clear();
        _stop_conditions.clear();
        _window_done = false;
        _capturing = true;
    }
}

void TreeProfiler::clear()
{
    auto l = _waitable.guard();
    for (auto & [channel, entry] : _channels)
        entry->notify_stat.clear();
    for (auto & [ctrl, entry] : _services)
        entry->op_stats.clear();
    _events.clear();
    for (auto & cond : _start_conditions)
        cond->satisfied = false;
    for (auto & cond : _stop_conditions)
        cond->satisfied = false;
    _window_done = false;
    _capturing = _start_conditions.empty();
    // start conditions may already be satisfied by the current state
    for (auto & cond : _start_conditions)
    {
        if (cond->attached)
            this->_evaluate_condition(cond);
    }
}

size_t TreeProfiler::channels_count() const
{
    auto l = _waitable.guard();
    return _channels.size();
}

size_t TreeProfiler::services_count() const
{
    auto l = _waitable.guard();
    return _services.size();
}

void TreeProfiler::handle(sihd::util::ServiceController *ctrl)
{
    std::optional<std::string> trace;
    sihd::util::AService *service = nullptr;
    ServiceEntry *entry = nullptr;
    std::string name;
    std::optional<Op> started_op;
    bool wait_for_walk = false;
    {
        auto l = _waitable.guard();
        entry = this->_find_service_entry(ctrl);
        if (entry == nullptr)
            return;
        service = entry->service;
        name = service_name(service);
        started_op = op_from_state(ctrl->state());
        // the op body runs after this notification returned: blocking here stops it
        // before it touches children; nested ops run on a thread already inside the walk
        wait_for_walk = _ops_depth.count(std::this_thread::get_id()) == 0;
        if (wait_for_walk == false)
        {
            trace = this->_handle_op(entry, ctrl, started_op, name);
            if (trace.has_value() == false)
                return;
        }
    }
    if (wait_for_walk)
    {
        auto l = _waitable.wait_guard([&] { return _walk_in_progress == false; });
        trace = this->_handle_op(entry, ctrl, started_op, name);
        if (trace.has_value() == false)
            return;
    }
    if (trace->empty() == false)
        SIHD_LOG(debug, "{}", *trace);
    // pick up children created during the op, without the profiler lock
    sihd::util::Node *node = dynamic_cast<sihd::util::Node *>(service);
    if (node != nullptr)
    {
        std::set<sihd::util::Node *> visited;
        this->_observe_node(node, 0, 0, visited);
    }
    this->_resolve_pending_conditions();
}

std::optional<std::string> TreeProfiler::_handle_op(ServiceEntry *entry,
                                                    sihd::util::ServiceController *ctrl,
                                                    const std::optional<Op> & started_op,
                                                    const std::string & name)
{
    if (started_op.has_value())
        return this->_handle_op_enter(entry, ctrl, *started_op, name);
    if (entry->op_pending)
        return this->_handle_op_exit(entry, ctrl, name);
    return std::nullopt;
}

void TreeProfiler::_push_ops_depth()
{
    _ops_depth[std::this_thread::get_id()] += 1;
    _ops_in_flight += 1;
}

void TreeProfiler::_pop_ops_depth()
{
    _ops_in_flight -= 1;
    auto it = _ops_depth.find(std::this_thread::get_id());
    if (it != _ops_depth.end() && --it->second == 0)
        _ops_depth.erase(it);
    _waitable.notify_all();
}

std::string TreeProfiler::_handle_op_enter(ServiceEntry *entry,
                                           sihd::util::ServiceController *ctrl,
                                           Op started_op,
                                           const std::string & name)
{
    this->_push_ops_depth();

    const WindowCheck window = this->_evaluate_window_conditions(ctrl);
    entry->op_begin = this->_now();
    entry->pending_op = started_op;
    entry->op_pending = true;
    entry->op_enter_recorded = window.captured();
    if (entry->op_enter_recorded == false)
        return "";

    const std::string what = op_str(started_op);
    this->_emit_enter(entry->op_begin, name, what);
    if (_trace == false)
        return "";
    return fmt::format("service '{}' op={} ...", name, what);
}

std::string
    TreeProfiler::_handle_op_exit(ServiceEntry *entry, sihd::util::ServiceController *ctrl, const std::string & name)
{
    this->_pop_ops_depth();

    const time::UnixTime elapsed = this->_now() - entry->op_begin;
    entry->op_pending = false;

    const WindowCheck window = this->_evaluate_window_conditions(ctrl);
    if (entry->op_enter_recorded == false && window.captured() == false)
        return "";

    // the notification that opened the window brings its missing enter along
    const Op ended_op = entry->pending_op;
    const std::string what = op_str(ended_op);
    if (entry->op_enter_recorded == false && window.was_open == false && window.opened)
        this->_emit_enter(entry->op_begin, name, what);

    const bool success = ctrl->state() != sihd::util::ServiceController::Error;
    this->_emit_exit(this->_now(), name, what, elapsed, success);

    entry->op_stats[ended_op].add_sample(elapsed);
    if (_trace == false)
        return "";
    return fmt::format("service '{}' op={} = {} ({})",
                       name,
                       what,
                       duration_str(elapsed),
                       success ? "success" : "error");
}

void TreeProfiler::_observe_node(sihd::util::Node *node,
                                 size_t current_recursion,
                                 size_t max_recursion,
                                 std::set<sihd::util::Node *> & visited)
{
    if (max_recursion != 0 && current_recursion == max_recursion)
        return;
    if (visited.insert(node).second == false)
        return;
    for (const std::string & name : node->children_keys())
    {
        sihd::util::Named *child = node->get_child(name);
        if (child == nullptr)
            continue;
        Channel *channel = dynamic_cast<Channel *>(child);
        if (channel != nullptr)
            this->_observe_channel(channel);
        sihd::util::AService *service = dynamic_cast<sihd::util::AService *>(child);
        if (service != nullptr)
            this->_observe_service(service);
        sihd::util::Node *child_node = dynamic_cast<sihd::util::Node *>(child);
        if (child_node != nullptr)
            this->_observe_node(child_node, current_recursion + 1, max_recursion, visited);
    }
}

bool TreeProfiler::_observe_channel(Channel *channel)
{
    // the map entry is claimed first: a concurrent observe finds it and never
    // installs its own entry, so the watcher slot is never stolen from an entry
    // that is about to be destroyed
    ChannelEntry *entry_ptr = nullptr;
    {
        auto l = _waitable.guard();
        if (_channels.count(channel) > 0)
            return true;
        auto entry = std::make_unique<ChannelEntry>();
        entry->channel = channel;
        entry->profiler = this;
        entry_ptr = entry.get();
        _channels.emplace(channel, std::move(entry));
    }
    if (channel->watcher() != nullptr)
    {
        SIHD_LOG_WARN("TreeProfiler: channel '{}' is already watched, it is not observed", channel->full_name());
        auto l = _waitable.guard();
        _channels.erase(channel);
        return false;
    }
    // lock order: set_watcher takes the channel lock
    channel->set_watcher(entry_ptr);
    if (channel->watcher() != entry_ptr)
    {
        auto l = _waitable.guard();
        _channels.erase(channel);
        return false;
    }
    return true;
}

bool TreeProfiler::_observe_service(sihd::util::AService *service)
{
    auto *ctrl = dynamic_cast<sihd::util::ServiceController *>(service->service_ctrl());
    if (ctrl == nullptr)
        return false;
    {
        auto l = _waitable.guard();
        if (_services.count(ctrl) > 0)
            return true;
    }
    if (ctrl->add_observer(this) == false)
        return false;
    auto entry = std::make_unique<ServiceEntry>();
    entry->service = service;
    entry->ctrl = ctrl;
    {
        auto l = _waitable.guard();
        if (_services.count(ctrl) > 0)
        {
            ctrl->remove_observer(this);
            return true;
        }
        ServiceEntry *entry_ptr = entry.get();
        _services.emplace(ctrl, std::move(entry));
        _service_entries.emplace(service, entry_ptr);
    }
    return true;
}

void TreeProfiler::_notify_begin(ChannelEntry *entry)
{
    std::string trace;
    {
        auto l = _waitable.guard();
        const Timestamp begin = this->_now();

        entry->_prune_stats();
        entry->write_detail.clear();

        const WindowCheck window = this->_evaluate_window_conditions(entry->channel);
        entry->write_in_flight = true;
        entry->write_begin = begin;
        entry->write_recorded = window.captured();
        if (entry->write_recorded)
        {
            this->_emit_enter(begin, entry->channel->full_name(), "write");
            if (_trace)
                trace = fmt::format("channel '{}' write ...", entry->channel->full_name());
        }
    }
    if (trace.empty() == false)
        SIHD_LOG(debug, "{}", trace);
}

void TreeProfiler::_notify_end(ChannelEntry *entry)
{
    std::string trace;
    {
        auto l = _waitable.guard();
        if (entry->write_in_flight == false)
            return;
        entry->write_in_flight = false;

        if (entry->write_recorded == false)
            return;

        const Timestamp end = this->_now();
        const time::UnixTime elapsed = end - entry->write_begin;
        entry->notify_stat.add_sample(elapsed);

        this->_emit_exit(end, entry->channel->full_name(), "write", elapsed, true);
        if (_trace)
        {
            trace = fmt::format("channel '{}' write = {} [{}]",
                                entry->channel->full_name(),
                                duration_str(elapsed),
                                fmt::join(entry->write_detail, ", "));
        }
    }
    if (trace.empty() == false)
        SIHD_LOG(debug, "{}", trace);
}

void TreeProfiler::_observer_begin(ChannelEntry *entry, sihd::util::IHandler<Channel *> *obs)
{
    auto l = _waitable.guard();
    if (entry->_find_stat(obs) == nullptr)
        entry->stats.push_back(ChannelEntry::ObsStat {obs, observer_label(obs), {}, 0});
    entry->pending_obs = obs;
    entry->pending_begin = this->_now();
}

void TreeProfiler::_observer_end(ChannelEntry *entry, sihd::util::IHandler<Channel *> *obs)
{
    auto l = _waitable.guard();
    if (entry->pending_obs != obs)
        return;
    entry->pending_obs = nullptr;

    ChannelEntry::ObsStat *stat = entry->_find_stat(obs);
    if (stat == nullptr)
        return;

    const time::UnixTime elapsed = this->_now() - entry->pending_begin;
    if (_capturing.load())
    {
        stat->stat.add_sample(elapsed);
        stat->last = elapsed;
    }
    if (_trace)
        entry->write_detail.push_back(fmt::format("{}={}", stat->label, duration_str(elapsed)));
}

bool TreeProfiler::start_when(Channel *channel, ChannelMatch match)
{
    return this->_when(true, channel, std::move(match));
}

bool TreeProfiler::start_when(Channel *channel, ChannelCondition cond)
{
    return this->_when(true, channel, std::move(cond));
}

bool TreeProfiler::start_when(sihd::util::AService *service, ServiceCondition cond)
{
    return this->_when(true, service, std::move(cond));
}

bool TreeProfiler::start_when(const std::string & path, ChannelMatch match)
{
    return this->_when(true, path, std::move(match));
}

bool TreeProfiler::start_when(const std::string & path, ChannelCondition cond)
{
    return this->_when(true, path, std::move(cond));
}

bool TreeProfiler::start_when(const std::string & path, ServiceCondition cond)
{
    return this->_when(true, path, std::move(cond));
}

bool TreeProfiler::stop_when(Channel *channel, ChannelMatch match)
{
    return this->_when(false, channel, std::move(match));
}

bool TreeProfiler::stop_when(Channel *channel, ChannelCondition cond)
{
    return this->_when(false, channel, std::move(cond));
}

bool TreeProfiler::stop_when(sihd::util::AService *service, ServiceCondition cond)
{
    return this->_when(false, service, std::move(cond));
}

bool TreeProfiler::stop_when(const std::string & path, ChannelMatch match)
{
    return this->_when(false, path, std::move(match));
}

bool TreeProfiler::stop_when(const std::string & path, ChannelCondition cond)
{
    return this->_when(false, path, std::move(cond));
}

bool TreeProfiler::stop_when(const std::string & path, ServiceCondition cond)
{
    return this->_when(false, path, std::move(cond));
}

bool TreeProfiler::_when(bool is_start, Channel *channel, ChannelMatch match)
{
    if (channel == nullptr)
        return false;
    CaptureCondition entry;
    entry.matcher = std::move(match);
    entry.channel = channel;
    return this->_add_condition(is_start, std::move(entry));
}

bool TreeProfiler::_when(bool is_start, Channel *channel, ChannelCondition cond)
{
    if (channel == nullptr || cond == nullptr)
        return false;
    CaptureCondition entry;
    entry.matcher = std::move(cond);
    entry.channel = channel;
    return this->_add_condition(is_start, std::move(entry));
}

bool TreeProfiler::_when(bool is_start, sihd::util::AService *service, ServiceCondition cond)
{
    if (service == nullptr || cond == nullptr)
        return false;
    CaptureCondition entry;
    entry.matcher = std::move(cond);
    entry.service = service;
    return this->_add_condition(is_start, std::move(entry));
}

bool TreeProfiler::_when(bool is_start, const std::string & path, ChannelMatch match)
{
    if (path.empty())
        return false;
    CaptureCondition entry;
    entry.matcher = std::move(match);
    entry.path = path;
    return this->_add_condition(is_start, std::move(entry));
}

bool TreeProfiler::_when(bool is_start, const std::string & path, ChannelCondition cond)
{
    if (path.empty() || cond == nullptr)
        return false;
    CaptureCondition entry;
    entry.matcher = std::move(cond);
    entry.path = path;
    return this->_add_condition(is_start, std::move(entry));
}

bool TreeProfiler::_when(bool is_start, const std::string & path, ServiceCondition cond)
{
    if (path.empty() || cond == nullptr)
        return false;
    CaptureCondition entry;
    entry.matcher = std::move(cond);
    entry.path = path;
    return this->_add_condition(is_start, std::move(entry));
}

TreeProfiler::ServiceCondition TreeProfiler::running()
{
    return [](sihd::util::ServiceController *ctrl) {
        return ctrl->state() == sihd::util::ServiceController::Running;
    };
}

TreeProfiler::ServiceCondition TreeProfiler::stopped()
{
    return [](sihd::util::ServiceController *ctrl) {
        return ctrl->state() == sihd::util::ServiceController::Stopped;
    };
}

void TreeProfiler::start_capture()
{
    auto l = _waitable.guard();
    _window_done = false;
    if (_capturing.load() == false)
        this->_open_window();
}

void TreeProfiler::stop_capture()
{
    auto l = _waitable.guard();
    if (_capturing.load())
        this->_close_window();
}

bool TreeProfiler::capturing() const
{
    return _capturing.load();
}

sihd::util::Duration TreeProfiler::session_duration() const
{
    auto l = _waitable.guard();
    if (_capturing.load())
        return _now() - _session_begin;
    return _last_session_duration;
}

void TreeProfiler::set_rearm(bool rearm)
{
    _rearm = rearm;
}

bool TreeProfiler::_add_condition(bool is_start, CaptureCondition && entry)
{
    entry.is_start = is_start;
    {
        auto l = _waitable.guard();
        ConditionList & conditions = is_start ? _start_conditions : _stop_conditions;
        // the first start condition closes the capture until it is satisfied
        if (is_start && conditions.empty() && _capturing.load())
            _capturing = false;
        conditions.push_back(std::make_shared<CaptureCondition>(std::move(entry)));
    }
    this->_resolve_pending_conditions();
    return true;
}

void TreeProfiler::_resolve_pending_conditions()
{
    std::vector<std::shared_ptr<CaptureCondition>> pending;
    {
        auto l = _waitable.guard();
        for (auto & cond : _start_conditions)
        {
            if (cond->attached == false)
                pending.push_back(cond);
        }
        for (auto & cond : _stop_conditions)
        {
            if (cond->attached == false)
                pending.push_back(cond);
        }
    }
    // lock order: subscribing takes the observable lock
    for (const std::shared_ptr<CaptureCondition> & cond : pending)
        this->_resolve_condition(cond);

    auto l = _waitable.guard();
    _start_conditions.remove_if([](const std::shared_ptr<CaptureCondition> & cond) { return cond->dropped; });
    _stop_conditions.remove_if([](const std::shared_ptr<CaptureCondition> & cond) { return cond->dropped; });
}

void TreeProfiler::_resolve_condition(const std::shared_ptr<CaptureCondition> & cond)
{
    {
        auto l = _waitable.guard();
        if (cond->attached || cond->dropped)
            return;
    }
    if (std::holds_alternative<ServiceCondition>(cond->matcher))
        this->_resolve_service_condition(cond);
    else
        this->_resolve_channel_condition(cond);
}

bool TreeProfiler::_resolve_channel_condition(const std::shared_ptr<CaptureCondition> & cond)
{
    Channel *channel = cond->channel;
    {
        auto l = _waitable.guard();
        if (channel == nullptr)
        {
            sihd::util::Named *named = this->_find_from_roots(cond->path);
            if (named == nullptr)
                return true;
            channel = dynamic_cast<Channel *>(named);
            if (channel == nullptr)
            {
                SIHD_LOG_WARN("TreeProfiler: '{}' is a '{}' and not a channel, condition dropped",
                              cond->path,
                              named->class_name());
                cond->dropped = true;
                return false;
            }
        }
        const ChannelMatch *match = std::get_if<ChannelMatch>(&cond->matcher);
        if (match != nullptr && match->verify(channel) == false)
        {
            cond->dropped = true;
            return false;
        }
    }
    if (this->_observe_channel(channel) == false)
    {
        SIHD_LOG_WARN("TreeProfiler: cannot observe channel '{}', condition dropped", channel->full_name());
        auto l = _waitable.guard();
        cond->dropped = true;
        return false;
    }
    {
        auto l = _waitable.guard();
        cond->channel = channel;
        cond->attached = true;
    }
    if (cond->is_start)
        this->_evaluate_condition(cond);
    return true;
}

bool TreeProfiler::_resolve_service_condition(const std::shared_ptr<CaptureCondition> & cond)
{
    sihd::util::AService *service = cond->service;
    {
        auto l = _waitable.guard();
        if (service == nullptr)
        {
            sihd::util::Named *named = this->_find_from_roots(cond->path);
            if (named == nullptr)
                return true;
            service = dynamic_cast<sihd::util::AService *>(named);
            if (service == nullptr)
            {
                SIHD_LOG_WARN("TreeProfiler: '{}' is a '{}' and not a service, condition dropped",
                              cond->path,
                              named->class_name());
                cond->dropped = true;
                return false;
            }
        }
    }
    auto *ctrl = dynamic_cast<sihd::util::ServiceController *>(service->service_ctrl());
    if (ctrl == nullptr)
    {
        SIHD_LOG_WARN("TreeProfiler: service '{}' has no service controller, condition dropped", service_name(service));
        auto l = _waitable.guard();
        cond->dropped = true;
        return false;
    }
    if (this->_observe_service(service) == false)
    {
        SIHD_LOG_WARN("TreeProfiler: cannot observe service '{}', condition dropped", service_name(service));
        auto l = _waitable.guard();
        cond->dropped = true;
        return false;
    }
    {
        auto l = _waitable.guard();
        cond->service = service;
        cond->ctrl = ctrl;
        cond->attached = true;
    }
    if (cond->is_start)
        this->_evaluate_condition(cond);
    return true;
}

sihd::util::Named *TreeProfiler::_find_from_roots(const std::string & path)
{
    for (sihd::util::Named *root : _roots)
    {
        sihd::util::Named *named = root->find(path);
        if (named == nullptr)
        {
            // accept the root name as a prefix: "root.parent" from the root "root"
            const std::string prefix = root->name() + Named::separator;
            if (path.rfind(prefix, 0) == 0)
                named = root->find(path.substr(prefix.size()));
        }
        if (named != nullptr)
            return named;
    }
    return nullptr;
}

bool TreeProfiler::WindowCheck::captured() const
{
    return was_open || opened;
}

TreeProfiler::WindowCheck TreeProfiler::_evaluate_window_conditions(Channel *channel)
{
    return this->_evaluate_window_conditions(channel, false);
}

TreeProfiler::WindowCheck TreeProfiler::_evaluate_window_conditions(sihd::util::ServiceController *ctrl)
{
    return this->_evaluate_window_conditions(ctrl, true);
}

TreeProfiler::WindowCheck TreeProfiler::_evaluate_window_conditions(const void *target, bool by_ctrl)
{
    WindowCheck check;
    check.was_open = _capturing.load();
    const size_t opens_before = _window_opens;
    this->_evaluate_conditions(target, by_ctrl);
    check.opened = _window_opens != opens_before;
    return check;
}

void TreeProfiler::_evaluate_conditions(const void *target, bool by_ctrl)
{
    // the predicates may add conditions, reset or clear: they run against a
    // snapshot so the lists below are never mutated while iterated
    std::vector<std::shared_ptr<CaptureCondition>> conds;
    {
        auto l = _waitable.guard();
        for (ConditionList *list : {&_start_conditions, &_stop_conditions})
        {
            for (const auto & cond : *list)
            {
                const void *key = by_ctrl ? static_cast<const void *>(cond->ctrl)
                                          : static_cast<const void *>(cond->channel);
                if (key == target)
                    conds.push_back(cond);
            }
        }
    }
    for (const std::shared_ptr<CaptureCondition> & cond : conds)
        this->_evaluate_condition(cond);
}

void TreeProfiler::_evaluate_condition(const std::shared_ptr<CaptureCondition> & cond)
{
    auto l = _waitable.guard();
    // start conditions wait for a closed window, stop conditions for an open one
    if (cond->attached == false)
        return;
    if (cond->is_start && (_capturing.load() || _window_done))
        return;
    if (cond->is_start == false && _capturing.load() == false)
        return;
    bool result = false;
    if (const auto *match = std::get_if<ChannelMatch>(&cond->matcher))
        result = match->match(cond->channel);
    else if (const auto *predicate = std::get_if<ChannelCondition>(&cond->matcher))
        result = (*predicate)(cond->channel);
    else
        result = std::get<ServiceCondition>(cond->matcher)(cond->ctrl);
    if (result)
        cond->satisfied = true;
    this->_update_window();
}

bool TreeProfiler::_all_satisfied(const ConditionList & conditions) const
{
    return std::all_of(conditions.begin(), conditions.end(), [](const auto & cond) { return cond->satisfied; });
}

void TreeProfiler::_update_window()
{
    if (_capturing.load() == false && _window_done == false && _start_conditions.empty() == false
        && this->_all_satisfied(_start_conditions))
        this->_open_window();
    if (_capturing.load() && _stop_conditions.empty() == false && this->_all_satisfied(_stop_conditions))
        this->_close_window();
}

void TreeProfiler::_open_window()
{
    _capturing = true;
    _window_opens += 1;
    _session_begin = this->_now();
    for (auto & cond : _stop_conditions)
        cond->satisfied = false;
    SIHD_LOG(debug, "TreeProfiler: capture started");
}

void TreeProfiler::_close_window()
{
    _last_session_duration = _now() - _session_begin;
    _capturing = false;
    for (auto & cond : _stop_conditions)
        cond->satisfied = false;
    if (_rearm.load())
    {
        for (auto & cond : _start_conditions)
            cond->satisfied = false;
    }
    else
    {
        _window_done = true;
    }
    SIHD_LOG(debug, "TreeProfiler: capture stopped");
}

std::string TreeProfiler::report_str() const
{
    return this->report_str({});
}

std::string TreeProfiler::report_str(const ReportOpts & opts) const
{
    {
        auto l = _waitable.guard();
        // an op body is the shadow zone of its own op: its report can never be awaited
        if (_ops_depth.count(std::this_thread::get_id()) > 0)
            return {};
    }
    // concurrent reports queue here, with the ops blocked in op_start
    auto l = _waitable.wait_guard([&] { return _walk_in_progress == false; });
    _walk_in_progress = true;
    l.unlock();
    l = _waitable.wait_guard([&] { return _ops_in_flight == 0; });
    l.unlock();
    WalkGuard walk_guard {_waitable, _walk_in_progress};
    // the report reflects every queued notification, or gives up when the
    // dispatch thread itself is held by this report (report from a hook)
    this->flush(sihd::util::time::milli(100));
    std::string s;
    {
        auto l = _waitable.guard();
        std::set<const sihd::util::Named *> visited;
        for (sihd::util::Named *root : _roots)
            this->_report_named(root, nullptr, root->name(), s, opts, 0, visited);
    }
    return s;
}

void TreeProfiler::log_report() const
{
    this->log_report({});
}

void TreeProfiler::log_report(const ReportOpts & opts) const
{
    SIHD_LOG(info, "tree profiler report:\n{}", this->report_str(opts));
}

std::vector<TreeProfiler::Event> TreeProfiler::events() const
{
    auto l = _waitable.guard();
    return std::vector<Event>(_events.begin(), _events.end());
}

std::string TreeProfiler::events_str() const
{
    std::string s;
    auto l = _waitable.guard();
    for (const Event & event : _events)
    {
        s += fmt::format("[{}] {} {} ", thread::id_str(event.thread_id), event.source, event.what);
        if (event.kind == Event::enter)
            s += "...";
        else
        {
            s += fmt::format("= {}", duration_str(event.duration));
            if (event.success == false)
                s += " (error)";
        }
        s += "\n";
    }
    return s;
}

void TreeProfiler::_emit_enter(sihd::util::Timestamp timestamp, const std::string & source, const std::string & what)
{
    this->_dispatch(this->_push_event(timestamp, source, what, Event::enter, 0, true));
}

void TreeProfiler::_emit_exit(sihd::util::Timestamp timestamp,
                              const std::string & source,
                              const std::string & what,
                              time::UnixTime duration,
                              bool success)
{
    this->_dispatch(this->_push_event(timestamp, source, what, Event::exit, duration, success));
}

TreeProfiler::Event TreeProfiler::_push_event(sihd::util::Timestamp timestamp,
                                              const std::string & source,
                                              const std::string & what,
                                              Event::Kind kind,
                                              time::UnixTime duration,
                                              bool success)
{
    if (_max_events > 0 && _events.size() >= _max_events)
        _events.pop_front();
    Event event;
    event.timestamp = timestamp;
    event.id = ++_event_counter;
    event.thread_id = thread::id();
    event.source = source;
    event.what = what;
    event.kind = kind;
    event.duration = duration;
    event.success = success;
    _events.push_back(event);
    return event;
}

void TreeProfiler::_report_named(const sihd::util::Named *named,
                                 const sihd::util::Node *parent,
                                 const std::string & key,
                                 std::string & s,
                                 const ReportOpts & opts,
                                 size_t depth,
                                 std::set<const sihd::util::Named *> & visited) const
{
    const Channel *channel = dynamic_cast<const Channel *>(named);
    const sihd::util::AService *service = dynamic_cast<const sihd::util::AService *>(named);
    const sihd::util::Node *node = dynamic_cast<const sihd::util::Node *>(named);
    // nodes are walked, so they are deduplicated to break cycles; linked channels
    // are leaves and show up under every alias
    if (node != nullptr && visited.emplace(named).second == false)
        return;
    const std::string indent(depth * 2, ' ');

    s += fmt::format("{}{}: {}", indent, key, named->class_name());
    if (parent != nullptr)
    {
        if (key != named->name())
            s += fmt::format("  => {}", named->full_name());
        else if (named->cparent() != parent)
            s += fmt::format("  -> {}", named->full_name());
    }
    const ServiceEntry *service_entry = service != nullptr ? this->_cfind_service_entry(service) : nullptr;
    if (service_entry != nullptr)
        s += fmt::format(" [{}]", sihd::util::ServiceController::state_str(service_entry->ctrl->state()));
    if (opts.description)
    {
        const std::string desc = named->description();
        if (desc.empty() == false)
            s += fmt::format(" - {}", desc);
    }
    s += "\n";

    if (service_entry != nullptr)
    {
        for (const auto & [op, stat] : service_entry->op_stats)
            s += fmt::format("{}  {}: {}\n", indent, op_str(op), stat_str(stat));
    }
    if (channel != nullptr)
    {
        auto it = _channels.find(const_cast<Channel *>(channel));
        if (it != _channels.end())
        {
            const ChannelEntry *entry = it->second.get();
            const sihd::util::Stat<time::UnixTime> & stat = entry->notify_stat;
            if (stat.samples > 0)
                s += fmt::format("{}  {}\n", indent, stat_str(stat, "writes"));
            for (const ChannelEntry::ObsStat & obs_stat : entry->stats)
                s += fmt::format("{}  -> {}: {}\n", indent, obs_stat.label, stat_str(obs_stat.stat));
        }
    }

    if (node == nullptr || (opts.max_recursion != 0 && depth >= opts.max_recursion))
        return;
    for (const std::string & name : node->children_keys())
    {
        const sihd::util::Named *child = node->cget_child(name);
        if (child != nullptr)
            this->_report_named(child, node, name, s, opts, depth + 1, visited);
    }
}

TreeProfiler::ServiceEntry *TreeProfiler::_find_service_entry(sihd::util::ServiceController *ctrl)
{
    auto it = _services.find(ctrl);
    return it != _services.end() ? it->second.get() : nullptr;
}

const TreeProfiler::ServiceEntry *TreeProfiler::_cfind_service_entry(const sihd::util::AService *service) const
{
    auto it = _service_entries.find(service);
    return it != _service_entries.end() ? it->second : nullptr;
}

void TreeProfiler::_collect_live(const sihd::util::Named *named,
                                 std::set<const sihd::util::Named *> & nameds,
                                 std::set<const sihd::util::AService *> & services) const
{
    if (named == nullptr || nameds.insert(named).second == false)
        return;
    const sihd::util::AService *service = dynamic_cast<const sihd::util::AService *>(named);
    if (service != nullptr)
        services.insert(service);
    const sihd::util::Node *node = dynamic_cast<const sihd::util::Node *>(named);
    if (node == nullptr)
        return;
    for (const std::string & name : node->children_keys())
        this->_collect_live(node->cget_child(name), nameds, services);
}

sihd::util::Timestamp TreeProfiler::_now() const
{
    return _clock_ptr != nullptr ? _clock_ptr->now() : _clock.now();
}

} // namespace sihd::core
