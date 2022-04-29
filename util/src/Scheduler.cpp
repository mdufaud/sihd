#include <sihd/util/Scheduler.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Scheduler);

SIHD_LOGGER;

Scheduler::Scheduler(const std::string & name, Node *parent): Named(name, parent)
{
    _clock_ptr = &_default_clock;
    overrun_at = time::micro(300);
    acceptable_nano = 100;
    _next_run = 0;
    _running = false;
    _paused = false;
    _waiting = false;
    _pausing = false;
    _no_delay = false;
    _paused_time = 0;
    this->add_conf("as_fast_as_possible", &Scheduler::set_as_fast_as_possible);
}

Scheduler::~Scheduler()
{
    this->stop();
    this->clear_tasks();
}

void    Scheduler::set_clock(IClock *ptr)
{
    _clock_ptr = ptr;
}

IClock  *Scheduler::get_clock() const
{
    return _clock_ptr;
}

bool    Scheduler::set_as_fast_as_possible(bool active)
{
    _no_delay = active;
    return true;
}

bool    Scheduler::_wait_for_next_task()
{
    Modificator w(_waiting, true);
    {
        if (_paused)
        {
            Modificator p(_pausing, true);
            time_t before = _clock_ptr->now();
            while (_running && _paused)
                _waitable_pause.infinite_wait();
            _paused_time += (_clock_ptr->now() - before);
        }
    }
    while (_running && _task_map.empty())
        _waitable_task.infinite_wait();
    if (_running == false)
        return false;
    return _no_delay
        ? true
        : _waitable_task.wait_for(_next_run - _clock_ptr->now() - this->acceptable_nano);
}

Task    *Scheduler::_get_next_task(time_t time)
{
    std::lock_guard l(_mutex_task);
    Task *task = _task_map.begin()->second;
    // difference between the time the task should have been played at (+paused time)
    // and current time
    time_t diff = (task->run_at + _paused_time) - time;
    if (task->run_at > 0 && -diff > (time_t)this->overrun_at)
        this->overruns += 1;
    // play task if near the time to be played or if no delay
    if ((diff - this->acceptable_nano) <= 0 || _no_delay)
        _task_map.erase(_task_map.begin());
    else
        task = nullptr;
    return task;
}

void    Scheduler::_play_task(Task *task, time_t now)
{
    task->run();
    if (task->resched_time > 0)
    {
        if (task->run_at == 0)
            task->run_at = now;
        task->run_at += task->resched_time;
        this->add_task(task);
    }
    else
        this->_add_to_delete_task(task);
}

time_t  Scheduler::now() const
{
    return _clock_ptr != nullptr ? _clock_ptr->now() : 0;
}

bool    Scheduler::run()
{
    sihd::util::Thread::set_name(this->get_name());
    if (_clock_ptr == nullptr || _clock_ptr->start() == false)
        return false;
    _begin_run = _clock_ptr->now();
    time_t now = _begin_run;
    Task *task = nullptr;
    while (_running)
    {
        this->_wait_for_next_task();
        if (_running && _paused == false)
        {
            now = _clock_ptr->now();
            // returns most urgent task to play
            task = this->_get_next_task(now);
            // run and reschedule or add to delete list
            if (task != nullptr)
                this->_play_task(task, now);
            // delete tasks in delete list
            this->_delete_tasks();
        }
    }
    _begin_run = 0;
    return true;
}

void    Scheduler::pause()
{
    _paused = true;
}

void    Scheduler::resume()
{
    _paused = false;
    // if in pause lock - try to notify wait until unlocked
    while (_pausing.load() == true)
        _waitable_pause.notify_all();
}

bool    Scheduler::start()
{
    if (_running.exchange(true) == true)
        return true;
    std::lock_guard l(_mutex_state);
    overruns = 0;
    _paused_time = 0;
    _thread = std::thread(&Scheduler::run, this);
    return true;
}

bool    Scheduler::stop()
{
    if (_running.exchange(false) == false)
        return true;
    std::lock_guard l(_mutex_state);
    _paused = false;
    while (_waiting.load() == true)
    {
        _waitable_pause.notify_all();
        _waitable_task.notify_all();
    }
    bool ret = _clock_ptr != nullptr && _clock_ptr->stop();
    if (_thread.joinable())
        _thread.join();
    return ret;
}

bool    Scheduler::is_running() const
{
    return _running;
}

void    Scheduler::add_task(Task *task)
{
    std::lock_guard l(_mutex_task);
    _task_map.insert(std::pair<time_t, Task *>(task->run_at, task));
    _next_run = _task_map.begin()->first;
    _waitable_task.notify_all();
}

void    Scheduler::remove_task(Task *task)
{
    std::lock_guard l(_mutex_task);
    for (auto it = _task_map.begin(); it != _task_map.end(); ++it)
    {
        if (it->second != nullptr && it->second == task)
        {
            _task_map.erase(it);
            break ;
        }
    }
    _next_run = _task_map.begin()->first;
    _waitable_task.notify_all();
}

void    Scheduler::clear_tasks()
{
    this->_delete_tasks();
    std::lock_guard l(_mutex_task);
    for (auto & pair : _task_map)
    {
        if (pair.second != nullptr)
            delete pair.second;
    }
    _task_map.clear();
    _waitable_task.notify_all();
}

void    Scheduler::_add_to_delete_task(Task *task)
{
    std::lock_guard l(_mutex_task);
    _task_rm_list.push_back(task);
}

void    Scheduler::_delete_tasks()
{
    std::lock_guard l(_mutex_task);
    for (Task *to_remove : _task_rm_list)
    {
        if (to_remove == nullptr)
            continue ;
        delete to_remove;
    }
    _task_rm_list.clear();
}

}