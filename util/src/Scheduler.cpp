#include <sihd/util/Scheduler.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Scheduler);

LOGGER;

Scheduler::Scheduler(const std::string & name, Node *parent): Named(name, parent) 
{
    overrun_at = time::micro(300);
    acceptable_nano = 100;
    _next_run = 0;
    _running = false;
    _clock_ptr = new SystemClock();
}

Scheduler::~Scheduler()
{
    this->stop();
    this->set_clock(nullptr);
    this->_delete_tasks();
    for (auto & pair : _task_map)
    {
        if (pair.second != nullptr)
            delete pair.second;
    }
}

void    Scheduler::set_clock(IClock *ptr)
{
    if (_clock_ptr)
        delete _clock_ptr;
    _clock_ptr = ptr;
}

IClock  *Scheduler::get_clock()
{
    return _clock_ptr;
}

bool    Scheduler::start()
{
    {
        std::lock_guard l(_mutex_run);
        if (_running == true)
            return false;
        _running = true;
    }
    overruns = 0;
    _thread = std::thread(&Scheduler::run, this);
    LOG_DEBUG("Scheduler: started");
    return true;
}

bool    Scheduler::stop()
{
    {
        std::lock_guard l(_mutex_run);
        if (_running == false)
            return false;
        _running = false;
    }
    _waitable.notify_all();
    bool ret = _clock_ptr != nullptr && _clock_ptr->stop();
    if (_thread.joinable())
        _thread.join();
    LOG_DEBUG("Scheduler: stopped");
    return ret;
}

bool    Scheduler::is_running()
{
    return _running;
}

bool    Scheduler::_wait_for(std::time_t wait_time)
{
    while (_task_map.empty() && _running)
        _waitable.infinite_wait();
    if (_running)
        return _waitable.wait_for(wait_time);
    return false;
}

Task    *Scheduler::_get_next_task(std::time_t time)
{
    std::lock_guard l(_mutex_task);
    Task *task = _task_map.begin()->second;

    std::time_t diff = task->run_at - (time + this->acceptable_nano);
    if (-diff > this->overrun_at)
        this->overruns += 1;

    if (task != nullptr && diff <= 0)
        _task_map.erase(_task_map.begin());
    else
        task = nullptr;
    return task;
}

void    Scheduler::_play_task(Task *task, std::time_t now)
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

bool    Scheduler::run()
{
    sihd::util::Thread::set_name(this->get_name());
    if (_clock_ptr == nullptr || _clock_ptr->start() == false)
        return false;
    _begin_run = _clock_ptr->now() - _steady_clock.now();
    std::time_t steady_time = _steady_clock.now() + _begin_run;
    Task *task = nullptr;
    while (_running)
    {
        this->_wait_for(_next_run - steady_time);
        if (_running)
        {
            steady_time = _steady_clock.now() + _begin_run;
            task = this->_get_next_task(steady_time);
            if (task != nullptr)
                this->_play_task(task, steady_time);
            this->_delete_tasks();
            steady_time = _steady_clock.now() + _begin_run;
        }
    }
    return true;
}

void    Scheduler::add_task(Task *task)
{
    std::lock_guard l(_mutex_task);
    _task_map.insert(std::pair<std::time_t, Task *>(task->run_at, task));
    _next_run = _task_map.begin()->first;
    _waitable.notify_all();
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
    _waitable.notify_all();
}

void    Scheduler::_add_to_delete_task(Task *task)
{
    std::lock_guard l(_mutex_task);
    _task_rm_list.push_back(task);
}

void    Scheduler::_delete_tasks()
{
    for (Task *to_remove : _task_rm_list)
    {
        if (to_remove == nullptr)
            continue ;
        delete to_remove;
    }
    _task_rm_list.clear();
}

}