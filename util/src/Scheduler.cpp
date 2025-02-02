#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Scheduler);

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

    this->add_conf("no_delay", &Scheduler::set_no_delay);
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

time_t Scheduler::now() const
{
    return _clock_ptr != nullptr ? _clock_ptr->now() : 0;
}

bool Scheduler::set_no_delay(bool active)
{
    if (this->is_running())
        throw std::logic_error("Cannot set 'as fast as possible' mode while running");
    _no_delay = active;
    return true;
}

void Scheduler::_wait_for_next_task()
{
    // wait for resume if paused
    _waitable_pause.wait([this] { return this->stop_requested || _paused == false; });
    // wait for new task if empty
    _waitable_task.wait([this] { return this->stop_requested || _task_map.empty() == false; });

    if (_no_delay)
        return;

    // wait until most recent task to play
    _waitable_task.wait_for(_next_run - _clock_ptr->now(), [this] { return this->stop_requested.load(); });
}

Task *Scheduler::_get_playable_task(time_t now)
{
    auto l = _waitable_task.guard();
    Task *task = _task_map.begin()->second;

    time_t diff = task->run_at - now;
    if (task->run_at > 0 && -diff > this->overrun_at)
        this->overruns += 1;

    // play task if near the time to be played or if in no delay mode
    if (_no_delay || (diff - this->acceptable_task_preplay_ns_time) <= 0)
        _task_map.erase(_task_map.begin());
    else
        task = nullptr;
    return task;
}

void Scheduler::_play_task(Task *task, time_t now)
{
    task->run();
    if (task->reschedule_time > 0)
    {
        if (task->run_at == 0)
            task->run_at = now;
        task->run_at += task->reschedule_time;

        {
            auto l = _waitable_task.guard();
            this->_unprotected_add_task_to_map(task);
        }
    }
    else
        this->_add_task_to_trash(task);
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
        _task_map.emplace(task->run_at, task);
    }

    if (_task_map.empty() == false)
        _next_run = _task_map.begin()->first;

    _tasks_to_add.clear();
    _tasks_prepared = true;
}

bool Scheduler::on_work_start()
{
    if (_clock_ptr == nullptr || _clock_ptr->start() == false)
        return false;

    this->overruns = 0;

    _begin_run = _clock_ptr->now();

    this->_prepare_tasks();

    time_t now = _begin_run;

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
    auto l = _waitable_pause.guard();
    if (_paused)
        return;
    _paused = true;
    _paused_time_at = this->now();
}

void Scheduler::_resume_tasks()
{
    if (_paused_time_at == 0)
        return;

    auto l = _waitable_task.guard();

    if (_tasks_prepared == false)
        return;

    const time_t paused_time = this->now() - std::max(_begin_run, _paused_time_at);
    _paused_time_at = 0;

    std::multimap<time_t, Task *> new_task_map;
    for (auto & [_, task] : _task_map)
    {
        if (task->run_in > 0)
        {
            task->run_at += paused_time;
        }
        new_task_map.emplace(task->run_at, task);
    }
    _task_map = std::move(new_task_map);
    if (_task_map.empty() == false)
        _next_run = _task_map.begin()->first;
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
}

void Scheduler::add_task(Task *task)
{
    auto l = _waitable_task.guard();

    if (_tasks_prepared)
    {
        if (task->run_in > 0)
            task->run_at = this->now() + task->run_in;
        this->_unprotected_add_task_to_map(task);
        _waitable_task.notify();
    }
    else
    {
        _tasks_to_add.push_back(task);
    }
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
        found = true;
        _next_run = _task_map.begin()->first;
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

    _waitable_task.notify();
}

void Scheduler::_add_task_to_trash(Task *task)
{
    auto l = _waitable_task.guard();
    _trash_task_list.push_back(task);
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
