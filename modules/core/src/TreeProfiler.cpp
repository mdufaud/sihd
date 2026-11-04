#include <algorithm>
#include <string_view>

#include <fmt/ranges.h>
#include <fmt/std.h>

#include <sihd/core/TreeProfiler.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::core
{

using namespace sihd::util;

namespace
{

using Op = AService::Operation;

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
    return Duration(nano).str();
}

std::string stat_str(const Stat<time::UnixTime> & stat, std::string_view count_label = "n")
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

} // namespace

SIHD_LOGGER;

TreeProfiler::TreeProfiler(TreeMonitor & monitor): _monitor(monitor)
{
    _session_begin = _monitor.now();
    _monitor.add_observer(this);
}

TreeProfiler::~TreeProfiler()
{
    _monitor.remove_observer(this);
}

void TreeProfiler::set_trace(bool active)
{
    _trace = active;
}

void TreeProfiler::set_max_events(size_t max_events)
{
    _max_events = max_events;
}

void TreeProfiler::set_rearm(bool rearm)
{
    _rearm = rearm;
}

void TreeProfiler::_flush() const
{
    _monitor.flush();
}

void TreeProfiler::reset()
{
    this->_flush();
    std::lock_guard lock(_mutex);
    _channels.clear();
    _services.clear();
    _events.clear();
    _start_conditions.clear();
    _stop_conditions.clear();
    _window_done = false;
    _capturing = true;
}

void TreeProfiler::clear()
{
    this->_flush();
    std::lock_guard lock(_mutex);
    for (auto & [channel, stats] : _channels)
        stats.notify_stat.clear();
    for (auto & [service, stats] : _services)
        stats.op_stats.clear();
    _events.clear();
    for (auto & cond : _start_conditions)
        cond->satisfied = false;
    for (auto & cond : _stop_conditions)
        cond->satisfied = false;
    _window_done = false;
    _capturing = _start_conditions.empty();
    if (_start_conditions.empty())
        return;
    const Timestamp now = _monitor.now();
    const ConditionList start_conditions = _start_conditions;
    for (const auto & cond : start_conditions)
    {
        if (this->_condition_active(*cond))
            this->_settle_condition(*cond, this->_matches_live(*cond), now);
    }
}

void TreeProfiler::handle(TreeMonitorEvent *event)
{
    if (event->kind == TreeMonitorEvent::channel_attached || event->kind == TreeMonitorEvent::service_attached)
    {
        this->_resolve_pending_conditions(event->timestamp, event);
        return;
    }
    if (event->kind == TreeMonitorEvent::channel_detached || event->kind == TreeMonitorEvent::service_detached)
    {
        this->_handle_detached(*event);
        return;
    }

    std::vector<Event> emitted;
    {
        std::lock_guard lock(_mutex);
        switch (event->kind)
        {
            case TreeMonitorEvent::write_begin:
                this->_handle_write_begin(*event, emitted);
                break;
            case TreeMonitorEvent::write_end:
                this->_handle_write_end(*event, emitted);
                break;
            case TreeMonitorEvent::op_begin:
                this->_handle_op_begin(*event, emitted);
                break;
            case TreeMonitorEvent::op_end:
                this->_handle_op_end(*event, emitted);
                break;
            default:
                break;
        }
    }
    for (Event & profiler_event : emitted)
        this->notify_observers(&profiler_event);
}

void TreeProfiler::_handle_write_begin(const TreeMonitorEvent & event, std::vector<Event> & emitted)
{
    const WindowCheck window = this->_evaluate_window_conditions(event);
    ChannelStats & stats = _channels[event.channel];
    stats.write_in_flight = true;
    stats.write_recorded = window.captured();
    if (stats.write_recorded == false)
        return;
    emitted.push_back(this->_push_event(event, event.timestamp, "write", Event::enter, 0, true));
    if (_trace)
        SIHD_LOG(debug, "channel '{}' write ...", *event.path);
}

void TreeProfiler::_handle_write_end(const TreeMonitorEvent & event, std::vector<Event> & emitted)
{
    auto it = _channels.find(event.channel);
    if (it == _channels.end())
        return;
    ChannelStats & stats = it->second;

    std::erase_if(stats.observers, [&event](const ObserverStat & obs_stat) {
        return std::none_of(event.observers->begin(), event.observers->end(), [&obs_stat](const auto & time) {
            return time.observer.handler == obs_stat.observer.handler;
        });
    });
    std::vector<std::string> details;
    for (const TreeMonitorEvent::ObserverTime & time : *event.observers)
    {
        auto stat_it = std::find_if(
            stats.observers.begin(),
            stats.observers.end(),
            [&time](const ObserverStat & obs_stat) { return obs_stat.observer.handler == time.observer.handler; });
        if (stat_it == stats.observers.end())
            stat_it = stats.observers.insert(stats.observers.end(), ObserverStat {time.observer, {}});
        if (_capturing.load())
            stat_it->stat.add_sample(time.duration);
        if (_trace)
            details.push_back(fmt::format("{}={}", *time.observer.label, duration_str(time.duration)));
    }

    if (stats.write_in_flight == false)
        return;
    stats.write_in_flight = false;
    if (stats.write_recorded == false)
        return;

    stats.notify_stat.add_sample(*event.duration);
    emitted.push_back(this->_push_event(event, event.timestamp, "write", Event::exit, *event.duration, true));
    if (_trace)
        SIHD_LOG(debug,
                 "channel '{}' write = {} [{}]",
                 *event.path,
                 duration_str(*event.duration),
                 fmt::join(details, ", "));
}

void TreeProfiler::_handle_op_begin(const TreeMonitorEvent & event, std::vector<Event> & emitted)
{
    const WindowCheck window = this->_evaluate_window_conditions(event);
    ServiceStats & stats = _services[event.service];
    stats.op_begin = event.timestamp;
    stats.pending_op = *event.operation;
    stats.op_pending = true;
    stats.op_enter_recorded = window.captured();
    if (stats.op_enter_recorded == false)
        return;

    const std::string what = op_str(*event.operation);
    emitted.push_back(this->_push_event(event, event.timestamp, what, Event::enter, 0, true));
    if (_trace)
        SIHD_LOG(debug, "service '{}' op={} ...", *event.path, what);
}

void TreeProfiler::_handle_op_end(const TreeMonitorEvent & event, std::vector<Event> & emitted)
{
    {
        auto it = _services.find(event.service);
        if (it == _services.end() || it->second.op_pending == false)
            return;
    }
    const WindowCheck window = this->_evaluate_window_conditions(event);
    // a predicate may have reset the profiler
    auto it = _services.find(event.service);
    if (it == _services.end())
        return;
    ServiceStats & stats = it->second;
    stats.op_pending = false;
    if (stats.op_enter_recorded == false && window.captured() == false)
        return;

    const std::string what = op_str(stats.pending_op);
    if (stats.op_enter_recorded == false && window.was_open == false && window.opened)
        emitted.push_back(this->_push_event(event, stats.op_begin, what, Event::enter, 0, true));
    emitted.push_back(this->_push_event(event, event.timestamp, what, Event::exit, *event.duration, *event.success));

    stats.op_stats[stats.pending_op].add_sample(*event.duration);
    if (_trace)
        SIHD_LOG(debug,
                 "service '{}' op={} = {} ({})",
                 *event.path,
                 what,
                 duration_str(*event.duration),
                 *event.success ? "success" : "error");
}

void TreeProfiler::_handle_detached(const TreeMonitorEvent & event)
{
    std::lock_guard lock(_mutex);
    if (event.channel != nullptr)
        _channels.erase(event.channel);
    else
        _services.erase(event.service);
    for (ConditionList *list : {&_start_conditions, &_stop_conditions})
    {
        for (const auto & cond : *list)
        {
            const bool targeted = event.channel != nullptr ? cond->channel == event.channel
                                                           : cond->service == event.service;
            if (targeted == false)
                continue;
            cond->attached = false;
            // a path waits for its next target, a pointer is gone with its object
            cond->dropped = cond->path.empty();
            cond->channel = nullptr;
            cond->service = nullptr;
        }
    }
    _start_conditions.remove_if([](const std::shared_ptr<CaptureCondition> & cond) { return cond->dropped; });
    _stop_conditions.remove_if([](const std::shared_ptr<CaptureCondition> & cond) { return cond->dropped; });
}

bool TreeProfiler::start_when(Channel *channel, ChannelMatch match)
{
    if (channel == nullptr)
        return false;
    return this->_when(true, {.matcher = std::move(match), .channel = channel});
}

bool TreeProfiler::start_when(Channel *channel, ChannelCondition cond)
{
    if (channel == nullptr || cond == nullptr)
        return false;
    return this->_when(true, {.matcher = std::move(cond), .channel = channel});
}

bool TreeProfiler::start_when(AService *service, ServiceCondition cond)
{
    if (service == nullptr || cond == nullptr)
        return false;
    return this->_when(true, {.matcher = std::move(cond), .service = service});
}

bool TreeProfiler::start_when(const std::string & path, ChannelMatch match)
{
    if (path.empty())
        return false;
    return this->_when(true, {.matcher = std::move(match), .path = path});
}

bool TreeProfiler::start_when(const std::string & path, ChannelCondition cond)
{
    if (path.empty() || cond == nullptr)
        return false;
    return this->_when(true, {.matcher = std::move(cond), .path = path});
}

bool TreeProfiler::start_when(const std::string & path, ServiceCondition cond)
{
    if (path.empty() || cond == nullptr)
        return false;
    return this->_when(true, {.matcher = std::move(cond), .path = path});
}

bool TreeProfiler::stop_when(Channel *channel, ChannelMatch match)
{
    if (channel == nullptr)
        return false;
    return this->_when(false, {.matcher = std::move(match), .channel = channel});
}

bool TreeProfiler::stop_when(Channel *channel, ChannelCondition cond)
{
    if (channel == nullptr || cond == nullptr)
        return false;
    return this->_when(false, {.matcher = std::move(cond), .channel = channel});
}

bool TreeProfiler::stop_when(AService *service, ServiceCondition cond)
{
    if (service == nullptr || cond == nullptr)
        return false;
    return this->_when(false, {.matcher = std::move(cond), .service = service});
}

bool TreeProfiler::stop_when(const std::string & path, ChannelMatch match)
{
    if (path.empty())
        return false;
    return this->_when(false, {.matcher = std::move(match), .path = path});
}

bool TreeProfiler::stop_when(const std::string & path, ChannelCondition cond)
{
    if (path.empty() || cond == nullptr)
        return false;
    return this->_when(false, {.matcher = std::move(cond), .path = path});
}

bool TreeProfiler::stop_when(const std::string & path, ServiceCondition cond)
{
    if (path.empty() || cond == nullptr)
        return false;
    return this->_when(false, {.matcher = std::move(cond), .path = path});
}

TreeProfiler::ServiceCondition TreeProfiler::running()
{
    return [](ServiceController::State state) {
        return state == ServiceController::Running;
    };
}

TreeProfiler::ServiceCondition TreeProfiler::stopped()
{
    return [](ServiceController::State state) {
        return state == ServiceController::Stopped;
    };
}

void TreeProfiler::start_capture()
{
    this->_flush();
    std::lock_guard lock(_mutex);
    _window_done = false;
    if (_capturing.load() == false)
        this->_open_window(_monitor.now());
}

void TreeProfiler::stop_capture()
{
    this->_flush();
    std::lock_guard lock(_mutex);
    if (_capturing.load())
        this->_close_window(_monitor.now());
}

bool TreeProfiler::capturing() const
{
    this->_flush();
    return _capturing.load();
}

Duration TreeProfiler::session_duration() const
{
    this->_flush();
    std::lock_guard lock(_mutex);
    if (_capturing.load())
        return _monitor.now() - _session_begin;
    return _last_session_duration;
}

bool TreeProfiler::_when(bool is_start, CaptureCondition && entry)
{
    this->_flush();
    entry.is_start = is_start;
    {
        std::lock_guard lock(_mutex);
        ConditionList & conditions = is_start ? _start_conditions : _stop_conditions;
        if (is_start && conditions.empty() && _capturing.load())
            _capturing = false;
        conditions.push_back(std::make_shared<CaptureCondition>(std::move(entry)));
    }
    this->_resolve_pending_conditions(_monitor.now(), nullptr);
    return true;
}

void TreeProfiler::_resolve_pending_conditions(Timestamp now, const TreeMonitorEvent *event)
{
    std::vector<std::shared_ptr<CaptureCondition>> pending;
    {
        std::lock_guard lock(_mutex);
        for (ConditionList *list : {&_start_conditions, &_stop_conditions})
        {
            for (const auto & cond : *list)
            {
                if (cond->attached == false && cond->dropped == false)
                    pending.push_back(cond);
            }
        }
    }
    // lock order: resolving takes the monitor tree lock
    for (const std::shared_ptr<CaptureCondition> & cond : pending)
        this->_resolve_condition(cond, now, event);

    std::lock_guard lock(_mutex);
    _start_conditions.remove_if([](const std::shared_ptr<CaptureCondition> & cond) { return cond->dropped; });
    _stop_conditions.remove_if([](const std::shared_ptr<CaptureCondition> & cond) { return cond->dropped; });
}

void TreeProfiler::_resolve_condition(const std::shared_ptr<CaptureCondition> & cond,
                                      Timestamp now,
                                      const TreeMonitorEvent *event)
{
    const bool is_service = std::holds_alternative<ServiceCondition>(cond->matcher);
    Channel *channel = nullptr;
    AService *service = nullptr;
    std::string path;
    {
        std::lock_guard lock(_mutex);
        if (cond->attached || cond->dropped)
            return;
        channel = cond->channel;
        service = cond->service;
        path = cond->path;
    }

    if (channel == nullptr && service == nullptr)
    {
        Named *named = _monitor.find(path);
        if (named == nullptr)
            return;
        channel = is_service ? nullptr : dynamic_cast<Channel *>(named);
        service = is_service ? dynamic_cast<AService *>(named) : nullptr;
        if (channel == nullptr && service == nullptr)
        {
            SIHD_LOG(warning,
                     "TreeProfiler: '{}' is a '{}' and not a {}, condition dropped",
                     path,
                     named->class_name(),
                     is_service ? "service" : "channel");
            std::lock_guard lock(_mutex);
            cond->dropped = true;
            return;
        }
    }
    // from the dispatch thread a condition waits for the attach event of its own target, which carries its state
    const bool attached = event != nullptr ? (is_service ? event->service == service : event->channel == channel)
                                           : (is_service ? _monitor.observes(service) : _monitor.observes(channel));
    if (attached == false)
        return;
    const bool valid = this->_verify_match(*cond, channel, event);

    std::lock_guard lock(_mutex);
    if (cond->attached || cond->dropped)
        return;
    if (valid == false)
    {
        cond->dropped = true;
        return;
    }
    cond->channel = channel;
    cond->service = service;
    cond->attached = true;
    if (cond->is_start && this->_condition_active(*cond))
    {
        const bool result = event != nullptr ? this->_matches_event(*cond, *event) : this->_matches_live(*cond);
        this->_settle_condition(*cond, result, now);
    }
}

bool TreeProfiler::_verify_match(const CaptureCondition & cond,
                                 const Channel *channel,
                                 const TreeMonitorEvent *event) const
{
    const ChannelMatch *match = std::get_if<ChannelMatch>(&cond.matcher);
    if (match == nullptr)
        return true;
    if (event != nullptr ? event->value != nullptr && match->verify(*event->value) : match->verify(channel))
        return true;
    SIHD_LOG(warning,
             "TreeProfiler: cannot match channel '{}', condition dropped",
             event != nullptr ? *event->path : channel->full_name());
    return false;
}

bool TreeProfiler::WindowCheck::captured() const
{
    return was_open || opened;
}

TreeProfiler::WindowCheck TreeProfiler::_evaluate_window_conditions(const TreeMonitorEvent & event)
{
    WindowCheck check;
    check.was_open = _capturing.load();
    const size_t opens_before = _window_opens;

    // predicates may add conditions, reset or clear: they run against a snapshot of the lists
    std::vector<std::shared_ptr<CaptureCondition>> conds;
    for (ConditionList *list : {&_start_conditions, &_stop_conditions})
    {
        for (const auto & cond : *list)
        {
            const bool targeted = event.channel != nullptr ? cond->channel == event.channel
                                                           : cond->service == event.service;
            if (targeted)
                conds.push_back(cond);
        }
    }
    for (const std::shared_ptr<CaptureCondition> & cond : conds)
    {
        if (this->_condition_active(*cond))
            this->_settle_condition(*cond, this->_matches_event(*cond, event), event.timestamp);
    }

    check.opened = _window_opens != opens_before;
    return check;
}

bool TreeProfiler::_matches_event(const CaptureCondition & cond, const TreeMonitorEvent & event) const
{
    if (const auto *predicate = std::get_if<ServiceCondition>(&cond.matcher))
        return event.state.has_value() && (*predicate)(*event.state);
    if (event.value == nullptr)
        return false;
    if (const auto *match = std::get_if<ChannelMatch>(&cond.matcher))
        return match->match(*event.value);
    return std::get<ChannelCondition>(cond.matcher)(*event.value);
}

bool TreeProfiler::_matches_live(const CaptureCondition & cond) const
{
    if (const auto *predicate = std::get_if<ServiceCondition>(&cond.matcher))
    {
        auto *ctrl = dynamic_cast<ServiceController *>(cond.service->service_ctrl());
        return ctrl != nullptr && (*predicate)(ctrl->state());
    }
    if (const auto *match = std::get_if<ChannelMatch>(&cond.matcher))
        return match->match(cond.channel);
    IArrayUnique copy = cond.channel->clone_array();
    return copy != nullptr && std::get<ChannelCondition>(cond.matcher)(*copy);
}

bool TreeProfiler::_condition_active(const CaptureCondition & cond) const
{
    if (cond.attached == false)
        return false;
    if (cond.is_start)
        return _capturing.load() == false && _window_done == false;
    return _capturing.load();
}

void TreeProfiler::_settle_condition(CaptureCondition & cond, bool result, Timestamp now)
{
    if (this->_condition_active(cond) == false)
        return;
    if (result)
        cond.satisfied = true;
    this->_update_window(now);
}

bool TreeProfiler::_all_satisfied(const ConditionList & conditions) const
{
    return std::all_of(conditions.begin(), conditions.end(), [](const auto & cond) { return cond->satisfied; });
}

void TreeProfiler::_update_window(Timestamp now)
{
    if (_capturing.load() == false && _window_done == false && _start_conditions.empty() == false
        && this->_all_satisfied(_start_conditions))
        this->_open_window(now);
    if (_capturing.load() && _stop_conditions.empty() == false && this->_all_satisfied(_stop_conditions))
        this->_close_window(now);
}

void TreeProfiler::_open_window(Timestamp now)
{
    _capturing = true;
    _window_opens += 1;
    _session_begin = now;
    for (auto & cond : _stop_conditions)
        cond->satisfied = false;
    SIHD_LOG(debug, "TreeProfiler: capture started");
}

void TreeProfiler::_close_window(Timestamp now)
{
    _last_session_duration = now - _session_begin;
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
    std::string report;
    const bool walked = _monitor.walk(
        [&](Named *named, const Node *parent, const std::string & key, size_t depth) {
            this->_report_named(named, parent, key, depth, opts, report);
        },
        opts.max_recursion);
    if (walked == false)
        return {};
    return report;
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
    this->_flush();
    std::lock_guard lock(_mutex);
    return std::vector<Event>(_events.begin(), _events.end());
}

std::string TreeProfiler::events_str() const
{
    this->_flush();
    std::string out;
    std::lock_guard lock(_mutex);
    for (const Event & event : _events)
    {
        out += fmt::format("[{}] {} {} ", event.thread_id, event.source, event.what);
        if (event.kind == Event::enter)
            out += "...";
        else
        {
            out += fmt::format("= {}", duration_str(event.duration));
            if (event.success == false)
                out += " (error)";
        }
        out += "\n";
    }
    return out;
}

TreeProfiler::Event TreeProfiler::_push_event(const TreeMonitorEvent & source_event,
                                              Timestamp timestamp,
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
    event.thread_id = source_event.thread_id;
    event.source = *source_event.path;
    event.what = what;
    event.kind = kind;
    event.duration = duration;
    event.success = success;
    _events.push_back(event);
    return event;
}

void TreeProfiler::_report_named(Named *named,
                                 const Node *parent,
                                 const std::string & key,
                                 size_t depth,
                                 const ReportOpts & opts,
                                 std::string & s) const
{
    const Channel *channel = dynamic_cast<const Channel *>(named);
    AService *service = dynamic_cast<AService *>(named);
    auto *ctrl = service != nullptr ? dynamic_cast<ServiceController *>(service->service_ctrl()) : nullptr;
    const std::string indent(depth * 2, ' ');

    s += fmt::format("{}{}: {}", indent, key, named->class_name());
    if (parent != nullptr)
    {
        if (key != named->name())
            s += fmt::format("  => {}", named->full_name());
        else if (named->parent() != parent)
            s += fmt::format("  -> {}", named->full_name());
    }
    if (ctrl != nullptr)
        s += fmt::format(" [{}]", ServiceController::state_str(ctrl->state()));
    if (opts.description)
    {
        const std::string desc = named->description();
        if (desc.empty() == false)
            s += fmt::format(" - {}", desc);
    }
    s += "\n";

    std::lock_guard lock(_mutex);
    if (auto it = _services.find(service); service != nullptr && it != _services.end())
    {
        for (const auto & [op, stat] : it->second.op_stats)
            s += fmt::format("{}  {}: {}\n", indent, op_str(op), stat_str(stat));
    }
    if (auto it = _channels.find(channel); channel != nullptr && it != _channels.end())
    {
        const ChannelStats & stats = it->second;
        if (stats.notify_stat.samples > 0)
            s += fmt::format("{}  {}\n", indent, stat_str(stats.notify_stat, "writes"));
        for (const ObserverStat & obs_stat : stats.observers)
            s += fmt::format("{}  -> {}: {}\n", indent, *obs_stat.observer.label, stat_str(obs_stat.stat));
    }
}

} // namespace sihd::core
