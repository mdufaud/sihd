#include <expected>

#include <fmt/format.h>

#include <sihd/util/Error.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/ThreadPool.hpp>
#include <sihd/util/thread.hpp>

namespace sihd::util
{

SIHD_LOGGER;

ThreadPool::ThreadPool(std::string_view name, size_t number_of_threads): _name(name)
{
    // creating threads
    _threads.reserve(number_of_threads);
    for (size_t i = 0; i < number_of_threads; ++i)
    {
        _threads.emplace_back(std::make_unique<Thread>(fmt::format("{}[{}]", name, i + 1), _jobs));
    }
}

ThreadPool::~ThreadPool()
{
    this->stop();
}

void ThreadPool::stop()
{
    _jobs.terminate();
    // pending jobs are discarded: their futures report a broken promise
    _jobs.clear();
    for (const auto & thread_ptr : _threads)
    {
        thread_ptr->stop();
    }
}

size_t ThreadPool::remaining_jobs() const
{
    return _jobs.size();
}

void ThreadPool::wait_all_jobs() const
{
    _jobs.wait_for_space(1);
}

std::vector<Stat<Duration>> ThreadPool::stats() const
{
    std::vector<Stat<Duration>> threads_stats;

    threads_stats.reserve(_threads.size());
    for (const auto & thread_ptr : _threads)
    {
        threads_stats.emplace_back(thread_ptr->stats());
    }
    return threads_stats;
}

ThreadPool::Thread::Thread(const std::string & name, SafeQueue<Job> & jobs): _jobs(jobs), _stop(false)
{
    _thread = std::thread([name, this] {
        SIHD_UNEXPECTED_LOG(thread::set_name(name));
        this->_loop();
    });
}

ThreadPool::Thread::~Thread()
{
    this->stop();
}

void ThreadPool::Thread::_loop()
{
    while (!_stop)
    {
        std::expected<Job, Error> expected_job = _jobs.pop();
        if (expected_job.has_value() == false)
            break;
        Job job = std::move(*expected_job);

        _stopwatch.reset();
        job();
        {
            std::lock_guard l(_stat_mutex);
            _jobs_stat.add_sample(_stopwatch.time());
        }
    }
}

void ThreadPool::Thread::stop()
{
    _stop = true;
    if (_thread.joinable())
    {
        _thread.join();
    }
}

Stat<Duration> ThreadPool::Thread::stats() const
{
    std::lock_guard l(_stat_mutex);
    return _jobs_stat;
}

} // namespace sihd::util
