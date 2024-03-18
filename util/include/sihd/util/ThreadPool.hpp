#ifndef __SIHD_UTIL_THREADPOOL_HPP__
#define __SIHD_UTIL_THREADPOOL_HPP__

#include <functional>
#include <future>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::util
{

class ThreadPool
{
    public:
        using Job = std::function<void()>;

        template <typename Func, typename... Args>
        using JobReturnType = typename std::invoke_result_t<Func, Args...>;

        struct Thread
        {
            public:
                Thread(const std::string & name, SafeQueue<Job> & jobs);
                ~Thread();

                void stop();
                Stat<Timestamp> stats() const;

            private:
                void _loop();

                bool _stop;
                SafeQueue<Job> & _jobs;
                Stat<Timestamp> _jobs_stat;
                std::thread _thread;
                mutable std::mutex _stat_mutex;
        };

        ThreadPool(std::string_view name, size_t number_of_threads);
        ~ThreadPool();

        template <typename Function>
        std::future<JobReturnType<Function>> add_job(Function && function)
        {
            using PackedTask = std::packaged_task<JobReturnType<Function>()>;

            auto packed_task = std::make_shared<PackedTask>(std::forward<Function>(function));
            auto future = packed_task->get_future();
            _jobs.push([this, packed_task] { (*packed_task)(); });
            return future;
        }

        void stop();
        void wait_all_jobs() const;
        size_t remaining_jobs() const;
        std::vector<Stat<Timestamp>> stats() const;

    protected:

    private:
        std::string _name;
        std::vector<std::unique_ptr<Thread>> _threads;
        SafeQueue<Job> _jobs;
};

} // namespace sihd::util

#endif
