#include <algorithm>
#include <typeinfo>

#include <sihd/core/TreeMonitor.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

namespace sihd::core
{

using namespace sihd::util;

namespace
{

using Op = AService::Operation;

struct WalkGuard
{
        WaitableRecursive & waitable;
        bool & walk_in_progress;

        ~WalkGuard()
        {
            {
                auto lock = waitable.guard();
                walk_in_progress = false;
            }
            waitable.notify_all();
        }
};

std::optional<Op> op_from_state(ServiceController::State state)
{
    switch (state)
    {
        case ServiceController::Configuring:
            return Op::Setup;
        case ServiceController::Initializing:
            return Op::Init;
        case ServiceController::Starting:
            return Op::Start;
        case ServiceController::Stopping:
            return Op::Stop;
        case ServiceController::Resetting:
            return Op::Reset;
        default:
            return std::nullopt;
    }
}

std::string observer_label(IHandler<Channel *> *obs)
{
    const std::string class_name = str::demangle(typeid(*obs).name());
    const Named *named = dynamic_cast<const Named *>(obs);
    if (named != nullptr)
        return fmt::format("{} ({})", named->name(), class_name);
    return class_name;
}

std::string service_name(const AService *service)
{
    const Named *named = dynamic_cast<const Named *>(service);
    if (named != nullptr)
        return named->full_name();
    return str::demangle(typeid(*service).name());
}

bool is_droppable(TreeMonitorEvent::Kind kind)
{
    return kind == TreeMonitorEvent::write_begin || kind == TreeMonitorEvent::write_end
           || kind == TreeMonitorEvent::op_begin || kind == TreeMonitorEvent::op_end;
}

} // namespace

SIHD_LOGGER;

void TreeMonitor::ChannelEntry::before_notification([[maybe_unused]] Channel *sender)
{
    monitor->_notify_begin(this);
}

void TreeMonitor::ChannelEntry::after_notification([[maybe_unused]] Channel *sender)
{
    monitor->_notify_end(this);
}

void TreeMonitor::ChannelEntry::before_observer([[maybe_unused]] IHandler<Channel *> *obs,
                                                [[maybe_unused]] Channel *sender)
{
    if (write_in_flight)
        observer_begin = monitor->now();
}

void TreeMonitor::ChannelEntry::after_observer(IHandler<Channel *> *obs, [[maybe_unused]] Channel *sender)
{
    if (write_in_flight == false)
        return;
    const Timestamp end = monitor->now();
    observers.push_back({this->_observer(obs), end - observer_begin});
}

const TreeMonitorEvent::Observer & TreeMonitor::ChannelEntry::_observer(IHandler<Channel *> *obs)
{
    auto it = std::find_if(labels.begin(), labels.end(), [obs](const TreeMonitorEvent::Observer & observer) {
        return observer.handler == obs;
    });
    if (it != labels.end())
        return *it;
    // sizes diverge only when an observer came or went
    if (labels.size() >= channel->observers_count())
    {
        std::set<const IHandler<Channel *> *> live;
        channel->for_each_observer([&live](IHandler<Channel *> *observer) { live.insert(observer); });
        std::erase_if(labels, [&live](const TreeMonitorEvent::Observer & observer) {
            return live.contains(observer.handler) == false;
        });
    }
    labels.push_back({obs, std::make_shared<const std::string>(observer_label(obs))});
    return labels.back();
}

TreeMonitor::TreeMonitor()
{
    _worker.set_method([this] {
        this->_dispatch_loop();
        return true;
    });
}

TreeMonitor::~TreeMonitor()
{
    this->_detach(false);
    this->_stop_dispatcher();
}

void TreeMonitor::set_clock(IClock *clock)
{
    _clock_ptr = clock;
}

Timestamp TreeMonitor::now() const
{
    return _clock_ptr != nullptr ? _clock_ptr->now() : _clock.now();
}

void TreeMonitor::set_queue_max(size_t queue_max)
{
    _queue_max = queue_max;
}

void TreeMonitor::set_queue_warning(size_t warning_threshold)
{
    _queue_warning = warning_threshold;
}

bool TreeMonitor::flush(Duration timeout) const
{
    if (_dispatcher_thread.load() == std::this_thread::get_id())
        return false;
    return _pending_waitable.wait_for(timeout, [this] { return _pending == 0; });
}

size_t TreeMonitor::dropped_events() const
{
    return _dropped_events_total;
}

bool TreeMonitor::add_observer(IHandler<Event *> *obs, bool add_to_front)
{
    const bool ret = Observable<Event>::add_observer(obs, add_to_front);
    if (ret)
        this->_ensure_dispatcher();
    return ret;
}

void TreeMonitor::remove_observer(IHandler<Event *> *obs)
{
    Observable<Event>::remove_observer(obs);
    // an in-flight dispatch may still hold obs in its snapshot
    std::lock_guard dispatching(_dispatch_mutex);
}

Named *TreeMonitor::find(const std::string & path) const
{
    auto lock = _waitable.guard();
    return this->_find_from_roots(path);
}

bool TreeMonitor::observes(const Channel *channel) const
{
    auto lock = _waitable.guard();
    return _channels.contains(const_cast<Channel *>(channel));
}

bool TreeMonitor::observes(const AService *service) const
{
    auto lock = _waitable.guard();
    return std::any_of(_services.begin(), _services.end(), [service](const auto & pair) {
        return pair.second->service == service;
    });
}

bool TreeMonitor::walk(const Visitor & visit, size_t max_recursion, Duration timeout) const
{
    {
        auto lock = _waitable.guard();
        if (_ops_depth.contains(std::this_thread::get_id()))
            return false;
    }

    auto walk_wait = _waitable.wait_for_guard(timeout, [this] { return _walk_in_progress == false; });
    if (walk_wait.timed_out)
        return false;
    _walk_in_progress = true;
    walk_wait.lock.unlock();

    WalkGuard walk_guard {_waitable, _walk_in_progress};
    auto ops_wait = _waitable.wait_for_guard(timeout, [this] { return _ops_in_flight == 0; });
    if (ops_wait.timed_out)
        return false;
    ops_wait.lock.unlock();

    // gives up when the dispatch thread itself runs this walk
    this->flush(Duration(time::milli(100)));

    auto tree_lock = _waitable.guard();
    std::set<const Named *> visited;
    for (const Root & root : _roots)
        this->_walk_named(root.named, nullptr, root.named->name(), visit, max_recursion, 0, visited);
    return true;
}

size_t TreeMonitor::channels_count() const
{
    auto lock = _waitable.guard();
    return _channels.size();
}

size_t TreeMonitor::services_count() const
{
    auto lock = _waitable.guard();
    return _services.size();
}

bool TreeMonitor::observe(Node *root, size_t max_recursion)
{
    if (root == nullptr)
        return false;
    // rooted first: subscribers resolve paths of the attached events
    this->_add_root(root, max_recursion);
    AService *service = dynamic_cast<AService *>(root);
    if (service != nullptr)
        this->_observe_service(service);
    std::set<Node *> visited;
    this->_observe_node(root, 0, max_recursion, visited);
    return true;
}

bool TreeMonitor::observe(Channel *channel)
{
    if (channel == nullptr)
        return false;
    this->_add_root(channel, std::nullopt);
    return this->_observe_channel(channel);
}

bool TreeMonitor::observe(AService *service)
{
    if (service == nullptr)
        return false;
    Named *named = dynamic_cast<Named *>(service);
    if (named != nullptr)
        this->_add_root(named, std::nullopt);
    return this->_observe_service(service);
}

void TreeMonitor::reset()
{
    this->_detach(true);
}

void TreeMonitor::_detach(bool notify)
{
    // liveness comes from a walk of the roots: stale entries of destroyed children are dropped undereferenced
    std::set<const Named *> nameds;
    std::set<const AService *> services;
    std::vector<Channel *> channels;
    std::vector<ServiceController *> ctrls;
    std::vector<Event> detached;
    {
        auto lock = _waitable.guard();
        for (const Root & root : _roots)
            this->_collect_live(root.named, nameds, services);

        for (auto & [channel, entry] : _channels)
        {
            if (nameds.contains(channel))
                channels.push_back(entry->channel);
            if (notify)
            {
                detached.push_back(this->_make_event(Event::channel_detached, entry->path, this->now()));
                detached.back().channel = channel;
            }
        }

        for (auto & [ctrl, entry] : _services)
        {
            if (services.contains(entry->service))
                ctrls.push_back(entry->ctrl);
            if (notify)
            {
                detached.push_back(this->_make_event(Event::service_detached, entry->path, this->now()));
                detached.back().service = entry->service;
            }
        }
        _roots.clear();
        _refused_channels.clear();
        _ops_in_flight = 0;
        _ops_depth.clear();
    }
    _waitable.notify_all();

    // lock order: set_watcher takes the channel lock
    for (Channel *channel : channels)
        channel->set_watcher(nullptr);
    for (ServiceController *ctrl : ctrls)
        ctrl->remove_observer(this);

    {
        auto lock = _waitable.guard();
        _channels.clear();
        _services.clear();
    }
    if (this->_has_subscribers())
    {
        std::ranges::sort(detached, {}, [](const Event & event) { return *event.path; });
        for (Event & event : detached)
            this->_dispatch(std::move(event));
    }
}

void TreeMonitor::handle(ServiceController *ctrl)
{
    const std::optional<Op> started_op = op_from_state(ctrl->state());
    Node *node = nullptr;
    {
        // blocking holds the op body before it touches children; a nested op is already inside the walk
        auto lock = _waitable.wait_guard(
            [this] { return _walk_in_progress == false || _ops_depth.contains(std::this_thread::get_id()); });
        auto it = _services.find(ctrl);
        if (it == _services.end() || this->_handle_op(it->second.get(), ctrl, started_op) == false)
            return;
        node = dynamic_cast<Node *>(it->second->service);
    }
    // without the tree lock (lock order)
    if (node != nullptr)
        this->_rescan(node);
}

bool TreeMonitor::_handle_op(ServiceEntry *entry, ServiceController *ctrl, const std::optional<Op> & started_op)
{
    if (started_op.has_value())
    {
        this->_handle_op_enter(entry, ctrl, *started_op);
        return true;
    }
    if (entry->op_pending)
    {
        this->_handle_op_exit(entry, ctrl);
        return true;
    }
    return false;
}

void TreeMonitor::_push_ops_depth()
{
    _ops_depth[std::this_thread::get_id()] += 1;
    _ops_in_flight += 1;
}

void TreeMonitor::_pop_ops_depth()
{
    _ops_in_flight -= 1;
    auto it = _ops_depth.find(std::this_thread::get_id());
    if (it != _ops_depth.end() && --it->second == 0)
        _ops_depth.erase(it);
    _waitable.notify_all();
}

void TreeMonitor::_handle_op_enter(ServiceEntry *entry, ServiceController *ctrl, Op started_op)
{
    this->_push_ops_depth();

    entry->op_begin = this->now();
    entry->pending_op = started_op;
    entry->op_pending = true;
    if (this->_has_subscribers() == false)
        return;

    Event event = this->_make_event(Event::op_begin, entry->path, entry->op_begin);
    event.service = entry->service;
    event.state = ctrl->state();
    event.operation = started_op;
    this->_dispatch(std::move(event));
}

void TreeMonitor::_handle_op_exit(ServiceEntry *entry, ServiceController *ctrl)
{
    this->_pop_ops_depth();

    entry->op_pending = false;
    if (this->_has_subscribers() == false)
        return;

    const ServiceController::State state = ctrl->state();
    Event event = this->_make_event(Event::op_end, entry->path, this->now());
    event.service = entry->service;
    event.state = state;
    event.operation = entry->pending_op;
    event.duration = event.timestamp - entry->op_begin;
    event.success = state != ServiceController::Failure && state != ServiceController::StartFailure
                    && state != ServiceController::StopFailure;
    this->_dispatch(std::move(event));
}

void TreeMonitor::_rescan(Node *node)
{
    this->_prune(node);
    const std::optional<size_t> max_recursion = this->_rescan_recursion(node);
    if (max_recursion.has_value() == false)
        return;
    std::set<Node *> visited;
    this->_observe_node(node, 0, *max_recursion, visited);
}

std::optional<size_t> TreeMonitor::_rescan_recursion(const Node *node) const
{
    std::optional<size_t> max_recursion;
    auto lock = _waitable.guard();
    for (const Root & root : _roots)
    {
        if (root.max_recursion.has_value() == false)
            continue;
        size_t depth = 0;
        const Named *named = node;
        while (named != nullptr && named != root.named)
        {
            named = named->parent();
            depth += 1;
        }
        if (named == nullptr)
            continue;
        if (*root.max_recursion == 0)
            return 0;
        if (depth < *root.max_recursion)
            max_recursion = std::max(max_recursion.value_or(0), *root.max_recursion - depth);
    }
    return max_recursion;
}

void TreeMonitor::_prune(const Node *node)
{
    const std::string prefix = node->full_name() + Named::separator;
    std::set<const Named *> nameds;
    std::set<const AService *> services;
    std::vector<std::pair<Channel *, ChannelEntry *>> stale_channels;
    std::vector<Event> detached;
    {
        auto lock = _waitable.guard();
        this->_collect_live(node, nameds, services);
        for (auto & [channel, entry] : _channels)
        {
            if (entry->watching && entry->path->starts_with(prefix))
                stale_channels.emplace_back(channel, entry.get());
        }

        for (auto it = _services.begin(); it != _services.end();)
        {
            if (it->second->path->starts_with(prefix) == false || services.contains(it->second->service))
            {
                ++it;
                continue;
            }
            detached.push_back(this->_make_event(Event::service_detached, it->second->path, this->now()));
            detached.back().service = it->second->service;
            it = _services.erase(it);
        }
    }

    // a live channel at a dead one's address does not carry its watcher
    std::erase_if(stale_channels, [&nameds](const std::pair<Channel *, ChannelEntry *> & pair) {
        return nameds.contains(pair.first) && pair.first->watcher() == pair.second;
    });

    {
        auto lock = _waitable.guard();
        for (auto & [channel, entry] : stale_channels)
        {
            auto it = _channels.find(channel);
            if (it == _channels.end() || it->second.get() != entry)
                continue;
            detached.push_back(this->_make_event(Event::channel_detached, entry->path, this->now()));
            detached.back().channel = channel;
            _channels.erase(it);
        }
    }

    if (this->_has_subscribers() == false)
        return;
    std::ranges::sort(detached, {}, [](const Event & event) { return *event.path; });
    for (Event & event : detached)
        this->_dispatch(std::move(event));
}

void TreeMonitor::_observe_node(Node *node, size_t current_recursion, size_t max_recursion, std::set<Node *> & visited)
{
    if (max_recursion != 0 && current_recursion == max_recursion)
        return;
    if (visited.insert(node).second == false)
        return;
    for (const std::string & name : node->children_keys())
    {
        Named *child = node->get_child(name);
        if (child == nullptr)
            continue;
        Channel *channel = dynamic_cast<Channel *>(child);
        if (channel != nullptr)
            this->_observe_channel(channel);
        AService *service = dynamic_cast<AService *>(child);
        if (service != nullptr)
            this->_observe_service(service);
        Node *child_node = dynamic_cast<Node *>(child);
        if (child_node != nullptr)
            this->_observe_node(child_node, current_recursion + 1, max_recursion, visited);
    }
}

bool TreeMonitor::_observe_channel(Channel *channel)
{
    auto entry = std::make_unique<ChannelEntry>();
    entry->channel = channel;
    entry->monitor = this;
    ChannelEntry *entry_ptr = entry.get();
    // the map entry is claimed first so a concurrent observe never steals the watcher slot of a dying entry
    {
        auto lock = _waitable.guard();
        if (_channels.contains(channel) || _refused_channels.contains(channel))
            return _channels.contains(channel);
        entry->path = std::make_shared<const std::string>(channel->full_name());
        _channels.emplace(channel, std::move(entry));
    }

    // lock order: set_watcher takes the channel lock
    if (channel->watcher() == nullptr)
        channel->set_watcher(entry_ptr);
    if (channel->watcher() != entry_ptr)
    {
        SIHD_LOG(warning, "TreeMonitor: channel '{}' is already watched, it is not observed", channel->full_name());
        auto lock = _waitable.guard();
        _channels.erase(channel);
        _refused_channels.insert(channel);
        return false;
    }

    std::shared_ptr<const std::string> path;
    {
        auto lock = _waitable.guard();
        auto it = _channels.find(channel);
        if (it != _channels.end() && it->second.get() == entry_ptr)
        {
            entry_ptr->watching = true;
            path = entry_ptr->path;
        }
    }
    // a concurrent reset dropped the entry
    if (path == nullptr)
    {
        channel->set_watcher(nullptr);
        return false;
    }

    if (this->_has_subscribers())
    {
        Event event = this->_make_event(Event::channel_attached, path, this->now());
        event.channel = channel;
        event.value = channel->clone_array();
        this->_dispatch(std::move(event));
    }
    return true;
}

bool TreeMonitor::_observe_service(AService *service)
{
    auto *ctrl = dynamic_cast<ServiceController *>(service->service_ctrl());
    if (ctrl == nullptr)
    {
        SIHD_LOG(debug, "TreeMonitor: service '{}' has no service controller", service_name(service));
        return false;
    }
    {
        auto lock = _waitable.guard();
        if (_services.contains(ctrl))
            return true;
    }

    if (ctrl->add_observer(this) == false)
    {
        SIHD_LOG(warning, "TreeMonitor: could not observe service '{}'", service_name(service));
        return false;
    }

    auto entry = std::make_unique<ServiceEntry>();
    entry->service = service;
    entry->ctrl = ctrl;
    entry->path = std::make_shared<const std::string>(service_name(service));
    const std::shared_ptr<const std::string> path = entry->path;
    {
        auto lock = _waitable.guard();
        if (_services.contains(ctrl))
        {
            ctrl->remove_observer(this);
            return true;
        }
        _services.emplace(ctrl, std::move(entry));
    }

    if (this->_has_subscribers())
    {
        Event event = this->_make_event(Event::service_attached, path, this->now());
        event.service = service;
        event.state = ctrl->state();
        this->_dispatch(std::move(event));
    }
    return true;
}

void TreeMonitor::_add_root(Named *root, std::optional<size_t> max_recursion)
{
    auto lock = _waitable.guard();
    auto it = std::find_if(_roots.begin(), _roots.end(), [root](const Root & r) { return r.named == root; });
    if (it == _roots.end())
        _roots.push_back({root, max_recursion});
    else if (it->max_recursion.has_value() == false)
        it->max_recursion = max_recursion;
}

void TreeMonitor::_notify_begin(ChannelEntry *entry)
{
    entry->observers.clear();
    entry->write_in_flight = this->_has_subscribers();
    if (entry->write_in_flight == false)
        return;

    Event event = this->_make_event(Event::write_begin, entry->path, this->now());
    event.channel = entry->channel;
    event.value = entry->channel->clone_array();
    entry->write_begin = event.timestamp;
    this->_dispatch(std::move(event));
}

void TreeMonitor::_notify_end(ChannelEntry *entry)
{
    if (entry->write_in_flight == false)
        return;
    entry->write_in_flight = false;

    Event event = this->_make_event(Event::write_end, entry->path, this->now());
    event.channel = entry->channel;
    event.duration = event.timestamp - entry->write_begin;
    event.observers = std::move(entry->observers);
    entry->observers.clear();
    this->_dispatch(std::move(event));
}

TreeMonitor::Event TreeMonitor::_make_event(Event::Kind kind,
                                            const std::shared_ptr<const std::string> & path,
                                            Timestamp timestamp) const
{
    Event event;
    event.kind = kind;
    event.timestamp = timestamp;
    event.thread_id = std::this_thread::get_id();
    event.path = path;
    return event;
}

bool TreeMonitor::_has_subscribers() const
{
    return Observable<Event>::observers_count() > 0;
}

void TreeMonitor::_ensure_dispatcher()
{
    if (_dispatcher_running.exchange(true))
        return;
    if (_worker.start_worker("sihd.treemonitor") == false)
    {
        SIHD_LOG(error, "TreeMonitor: could not start the dispatcher");
        _dispatcher_running = false;
    }
}

void TreeMonitor::_stop_dispatcher()
{
    if (_dispatcher_running.exchange(false) == false)
        return;
    _queue.terminate();
    _worker.stop_worker();
    _dispatcher_thread = std::thread::id();
}

void TreeMonitor::_dispatch(Event && event)
{
    const std::shared_ptr<const std::string> source = event.path;
    const size_t queue_max = is_droppable(event.kind) ? _queue_max.load() : 0;
    const size_t queue_size = _queue.size();
    {
        auto lock = _pending_waitable.guard();
        _pending += 1;
    }

    if (_queue.push(std::move(event), queue_max) == false)
    {
        bool first_drop = false;
        {
            auto lock = _pending_waitable.guard();
            _pending -= 1;
            first_drop = _errored_sources.insert(*source).second;
        }
        _pending_waitable.notify_all();
        _dropped_events_total += 1;
        if (first_drop)
            SIHD_LOG(error,
                     "TreeMonitor: dispatch queue full, events from '{}' are lost ({} dropped so far)",
                     *source,
                     _dropped_events_total.load());
        return;
    }

    const size_t warning_threshold = _queue_warning;
    if (warning_threshold == 0 || queue_size < warning_threshold)
        return;
    bool first_warning = false;
    {
        auto lock = _pending_waitable.guard();
        first_warning = _warned_sources.insert(*source).second;
    }
    if (first_warning)
        SIHD_LOG(warning,
                 "TreeMonitor: dispatch queue almost full ({}/{}), events from '{}' stack up",
                 queue_size,
                 _queue_max.load(),
                 *source);
}

void TreeMonitor::_dispatch_loop()
{
    _dispatcher_thread = std::this_thread::get_id();
    for (;;)
    {
        Result<Event> event = _queue.pop();
        if (event.has_value() == false)
            break;

        {
            // snapshot per event: subscribers may add or remove subscribers and run without the observable lock
            std::lock_guard dispatching(_dispatch_mutex);
            std::vector<IHandler<Event *> *> observers;
            this->for_each_observer([&observers](IHandler<Event *> *obs) { observers.push_back(obs); });
            for (IHandler<Event *> *obs : observers)
            {
                if (this->is_observer(obs))
                    obs->handle(&*event);
            }
        }

        {
            auto lock = _pending_waitable.guard();
            _pending -= 1;
            const size_t warning_threshold = _queue_warning;
            if (warning_threshold == 0 || _queue.size() < warning_threshold)
                _warned_sources.clear();
            if (_pending == 0)
                _errored_sources.clear();
        }
        _pending_waitable.notify_all();
    }
}

void TreeMonitor::_walk_named(Named *named,
                              const Node *parent,
                              const std::string & key,
                              const Visitor & visit,
                              size_t max_recursion,
                              size_t depth,
                              std::set<const Named *> & visited) const
{
    Node *node = dynamic_cast<Node *>(named);
    // nodes are deduplicated to break cycles; linked channels are leaves visited under every alias
    if (node != nullptr && visited.emplace(named).second == false)
        return;
    visit(named, parent, key, depth);
    if (node == nullptr || (max_recursion != 0 && depth >= max_recursion))
        return;
    for (const std::string & name : node->children_keys())
    {
        Named *child = node->get_child(name);
        if (child != nullptr)
            this->_walk_named(child, node, name, visit, max_recursion, depth + 1, visited);
    }
}

void TreeMonitor::_collect_live(const Named *named,
                                std::set<const Named *> & nameds,
                                std::set<const AService *> & services) const
{
    if (named == nullptr || nameds.insert(named).second == false)
        return;
    const AService *service = dynamic_cast<const AService *>(named);
    if (service != nullptr)
        services.insert(service);
    const Node *node = dynamic_cast<const Node *>(named);
    if (node == nullptr)
        return;
    for (const std::string & name : node->children_keys())
        this->_collect_live(node->get_child(name), nameds, services);
}

Named *TreeMonitor::_find_from_roots(const std::string & path) const
{
    for (const Root & root : _roots)
    {
        Named *named = root.named->find(path);
        if (named == nullptr)
        {
            // accept the root name as a prefix: "root.parent" from the root "root"
            const std::string prefix = root.named->name() + Named::separator;
            if (path.starts_with(prefix))
                named = root.named->find(path.substr(prefix.size()));
        }
        if (named != nullptr)
            return named;
    }
    return nullptr;
}

} // namespace sihd::core
