#include <sihd/util/Logger.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Scheduler::Scheduler(const std::string & name, Node *parent): Named(name, parent), AWorkerService(name)
{
    overrun_at = time::micro(300);
    acceptable_task_preplay_ns_time = 100;

    _clock_ptr = &_default_clock;

    _next_run = 0;
    _paused_time_at = 0;
    _paused = false;
    _no_delay = false;
    _tasks_prepared = false;
    _task_map_seq = 0;

    _idle_policy = IdlePolicy::sleep;
    _spin_window = time::micro(100);
    _skip_missed_on_start = false;

    this->add_conf("no_delay", &Scheduler::set_no_delay);
    this->add_conf("spin_window", &Scheduler::set_spin_window);
    this->add_conf("skip_missed_on_start", &Scheduler::set_skip_missed_on_start);
}

Scheduler::~Scheduler()
{
    if (this->is_running())
        this->stop();
    this->clear_tasks();
}

void Scheduler::set_clock(IClock *ptr)
{
    if (this->is_running())
        throw std::logic_error("Cannot set a clock while the scheduler is running");
    _clock_ptr = ptr;
}

IClock *Scheduler::clock() const
{
    return _clock_ptr;
}

Timestamp Scheduler::now() const
{
    return _clock_ptr != nullptr ? _clock_ptr->now() : Timestamp(0);
}

bool Scheduler::set_no_delay(bool active)
{
    if (this->is_running())
        throw std::logic_error("Cannot set 'as fast as possible' mode while running");
    _no_delay = active;
    return true;
}

bool Scheduler::set_idle_policy(IdlePolicy policy)
{
    if (this->is_running())
        throw std::logic_error("Cannot set idle policy while the scheduler is running");
    _idle_policy = policy;
    return true;
}

IdlePolicy Scheduler::idle_policy() const
{
    return _idle_policy;
}

bool Scheduler::set_spin_window(Duration window)
{
    if (this->is_running())
        throw std::logic_error("Cannot set spin window while the scheduler is running");
    if (window < 0)
        return false;
    _spin_window = window;
    return true;
}

Duration Scheduler::spin_window() const
{
    return _spin_window;
}

bool Scheduler::set_skip_missed_on_start(bool active)
{
    if (this->is_running())
        throw std::logic_error("Cannot set 'fast forward missed periods' while the scheduler is running");
    _skip_missed_on_start = active;
    return true;
}

bool Scheduler::skip_missed_on_start() const
{
    return _skip_missed_on_start;
}

void Scheduler::_wait_until_deadline(Duration delay, uint64_t seq)
{
    _waitable_task.wait_for(delay, [this, seq] {
        return this->stop_requested || _paused || _task_map.empty() || _task_map_seq.load() != seq;
    });
}

void Scheduler::_wait_for_next_task()
{
    // wait for resume if paused
    _waitable_pause.wait([this] { return this->stop_requested || _paused == false; });

    uint64_t seq = 0;
    Timestamp next_run_at = 0;
    // wait for new task if empty
    _waitable_task.wait([this] { return this->stop_requested || _task_map.empty() == false; });
    {
        // a generation change since this snapshot makes the timed wait below return early
        auto l = _waitable_task.guard();
        seq = _task_map_seq.load();
        next_run_at = _next_run;
    }

    if (_no_delay || next_run_at == 0)
        return;

    Duration delay = next_run_at - _clock_ptr->now();
    if (delay <= 0)
        return;

    if (_idle_policy == IdlePolicy::sleep_then_spin)
    {
        // sleep most of the wait on the condition variable, then poll the clock over the last
        // spin_window to cut wakeup latency
        Duration sleep_delay = delay - _spin_window;
        if (sleep_delay > 0)
            this->_wait_until_deadline(sleep_delay, seq);
        this->_spin_until(next_run_at, seq);
        return;
    }

    // sleep until the deadline - an earlier task, a pause or an emptied queue wakes us up
    this->_wait_until_deadline(delay, seq);
}

void Scheduler::_spin_until(Timestamp deadline, uint64_t seq)
{
    // poll the clock to cut wakeup latency - a non advancing (virtual) clock falls back to sleep
    Timestamp previous = 0;
    int frozen_polls = 0;
    int polls = 0;
    while (this->stop_requested == false && _paused == false)
    {
        Timestamp now = _clock_ptr->now();
        if (now >= deadline)
            return;
        if (now == previous)
        {
            if (++frozen_polls >= 128)
            {
                Duration delay = deadline - now;
                if (delay > 0)
                    this->_wait_until_deadline(delay, seq);
                return;
            }
        }
        else
        {
            frozen_polls = 0;
            previous = now;
        }
        // periodically recheck the queue state while spinning
        if (++polls % 64 == 0)
        {
            auto l = _waitable_task.guard();
            if (_task_map.empty() || _task_map_seq.load() != seq)
                return;
        }
    }
}

Task *Scheduler::_get_playable_task(Timestamp now)
{
    auto l = _waitable_task.guard();
    if (_task_map.empty())
        return nullptr;

    Task *task = _task_map.begin()->second;

    Duration diff = task->run_at - now;
    if (task->run_at > 0 && -diff > this->overrun_at)
        this->overruns += 1;

    // play task if near the time to be played or if in no delay mode
    if (_no_delay || (diff - this->acceptable_task_preplay_ns_time) <= 0)
    {
        _task_map.erase(_task_map.begin());
        // or the loop spins on a stale deadline
        _next_run = _task_map.empty() ? Timestamp(0) : _task_map.begin()->first;
    }
    else
        task = nullptr;
    return task;
}

void Scheduler::_play_task(Task *task, Timestamp now)
{
    try
    {
        task->run();
    }
    catch (const std::exception & err)
    {
        SIHD_LOG_ERROR("Scheduler '{}': task raised exception: {}", this->name(), err.what());
    }
    catch (...)
    {
        SIHD_LOG_ERROR("Scheduler '{}': task raised unknown exception", this->name());
    }

    // reschedule or trash in one critical section
    auto l = _waitable_task.guard();
    if (task->reschedule_time > 0)
    {
        if (task->run_at == 0)
            task->run_at = now;
        switch (task->late_policy)
        {
            case LatenessPolicy::push_back:
                task->run_at = now + task->reschedule_time;
                break;
            case LatenessPolicy::skip_missed:
                if (task->run_at <= now)
                    task->run_at += (((now - task->run_at) / task->reschedule_time) + 1) * task->reschedule_time;
                else
                    task->run_at += task->reschedule_time;
                break;
            case LatenessPolicy::replay_missed:
            default:
                task->run_at += task->reschedule_time;
                break;
        }
        this->_unprotected_add_task_to_map(task);
    }
    else
    {
        _trash_task_list.push_back(task);
    }
}

void Scheduler::_prepare_tasks()
{
    auto l = _waitable_task.guard();
    for (Task *task : _tasks_to_add)
    {
        if (task->run_in > 0)
        {
            task->run_at = _begin_run + task->run_in;
        }
        else if (_skip_missed_on_start && task->reschedule_time > 0 && task->run_at != 0
                 && task->run_at < _begin_run)
        {
            // jump to the next future slot instead of replaying missed runs
            task->run_at += (((_begin_run - task->run_at) / task->reschedule_time) + 1) * task->reschedule_time;
        }
        _task_map.emplace(task->run_at, task);
    }

    _next_run = _task_map.empty() ? Timestamp(0) : _task_map.begin()->first;

    _tasks_to_add.clear();
    _task_map_seq.fetch_add(1, std::memory_order_release);
    _tasks_prepared = true;
}

bool Scheduler::on_work_start()
{
    if (_clock_ptr == nullptr || _clock_ptr->start() == false)
        return false;

    this->overruns = 0;

    _begin_run = _clock_ptr->now();

    this->_prepare_tasks();

    Timestamp now = _begin_run;

    Task *task = nullptr;
    while (this->stop_requested == false)
    {
        this->_wait_for_next_task();
        if (this->stop_requested == false && _paused == false)
        {
            now = _clock_ptr->now();
            // returns most urgent task to play
            task = this->_get_playable_task(now);
            // run and reschedule or add to delete list
            if (task != nullptr)
                this->_play_task(task, now);
            // delete tasks in delete list
            this->_delete_trashed_tasks();
        }
    }

    return true;
}

void Scheduler::pause()
{
    {
        auto l = _waitable_pause.guard();
        if (_paused)
            return;
        _paused = true;
        _paused_time_at = this->now();
    }
    // wake a worker sleeping toward a far deadline so it joins the pause wait promptly
    _waitable_task.notify();
}

void Scheduler::_resume_tasks()
{
    if (_paused_time_at == 0)
        return;

    auto l = _waitable_task.guard();

    if (_tasks_prepared == false)
        return;

    const Duration paused_time = this->now() - std::max(_begin_run, _paused_time_at);
    _paused_time_at = 0;

    std::multimap<Timestamp, Task *> new_task_map;
    for (auto & [_, task] : _task_map)
    {
        if (task->run_in > 0)
        {
            task->run_at += paused_time;
        }
        new_task_map.emplace(task->run_at, task);
    }
    _task_map = std::move(new_task_map);
    _next_run = _task_map.empty() ? Timestamp(0) : _task_map.begin()->first;
    _task_map_seq.fetch_add(1, std::memory_order_release);
}

void Scheduler::resume()
{
    auto l = _waitable_pause.guard();

    if (_paused == false)
        return;

    this->_resume_tasks();

    _paused = false;
    _waitable_pause.notify();
}

bool Scheduler::on_work_stop()
{
    {
        auto l = _waitable_pause.guard();
        _paused = false;
        _waitable_pause.notify();
    }
    {
        auto l = _waitable_task.guard();
        _begin_run = 0;
        _waitable_task.notify();
    }
    if (_clock_ptr != nullptr)
        _clock_ptr->stop();
    return true;
}

void Scheduler::_unprotected_add_task_to_map(Task *task)
{
    _task_map.emplace(task->run_at, task);
    _next_run = _task_map.begin()->first;
    _task_map_seq.fetch_add(1, std::memory_order_release);
}

void Scheduler::add_task(Task *task)
{
    bool wake_worker = false;
    {
        auto l = _waitable_task.guard();

        if (_tasks_prepared)
        {
            if (task->run_in > 0)
                task->run_at = this->now() + task->run_in;
            // wake the worker only if the new task becomes the nearest deadline
            wake_worker = _task_map.empty() || task->run_at < _next_run;
            this->_unprotected_add_task_to_map(task);
        }
        else
        {
            _tasks_to_add.push_back(task);
        }
    }
    if (wake_worker)
        _waitable_task.notify();
}

bool Scheduler::remove_task(Task *task)
{
    auto l = _waitable_task.guard();

    bool found = false;

    const auto task_it = container::find(_tasks_to_add, task);
    if (task_it != _tasks_to_add.end())
    {
        _tasks_to_add.erase(task_it);
        found = true;
    }

    const auto map_it = container::find_if(_task_map, [&task](const auto & pair) { return task == pair.second; });
    if (map_it != _task_map.end())
    {
        _task_map.erase(map_it);
        _next_run = _task_map.empty() ? Timestamp(0) : _task_map.begin()->first;
        _task_map_seq.fetch_add(1, std::memory_order_release);
        found = true;
    }

    _waitable_task.notify();
    return found;
}

void Scheduler::clear_tasks()
{
    this->_delete_trashed_tasks();

    auto l = _waitable_task.guard();

    for (Task *task : _tasks_to_add)
    {
        if (task != nullptr)
            delete task;
    }
    _tasks_to_add.clear();

    for (auto & [_, task] : _task_map)
    {
        if (task != nullptr)
            delete task;
    }
    _task_map.clear();
    _next_run = 0;
    _task_map_seq.fetch_add(1, std::memory_order_release);

    _waitable_task.notify();
}

void Scheduler::_delete_trashed_tasks()
{
    auto l = _waitable_task.guard();
    for (Task *to_remove : _trash_task_list)
    {
        if (to_remove == nullptr)
            continue;
        delete to_remove;
    }
    _trash_task_list.clear();
}

} // namespace sihd::util
