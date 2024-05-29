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

        template <typename Function, typename... Args>
        using CallableReturnType = typename std::invoke_result_t<Function, Args...>;

        template <typename T>
        using VoidOrVectorReturnType = typename std::conditional_t<std::is_void_v<T>, void, std::vector<T>>;

        template <typename Function>
        using CompleteAllJobsReturnType = VoidOrVectorReturnType<CallableReturnType<Function, size_t>>;

        struct Thread
        {
            public:
                Thread(const std::string & name, SafeQueue<Job> & jobs);
                ~Thread();

                void stop();
                Stat<Timestamp> stats() const;

            private:
                void _loop();

                SafeQueue<Job> & _jobs;
                Stat<Timestamp> _jobs_stat;
                Stopwatch _stopwatch;
                bool _stop;
                std::thread _thread;
                mutable std::mutex _stat_mutex;
        };

        ThreadPool(std::string_view name, size_t number_of_threads);
        ~ThreadPool();

        template <typename Function>
        std::future<CallableReturnType<Function>> add_job(Function && function)
        {
            // wrap a callable target with std::packaged_task
            using PackagedTask = std::packaged_task<CallableReturnType<Function>()>;

            auto packed_task = std::make_shared<PackagedTask>(std::forward<Function>(function));
            _jobs.push([packed_task] { (*packed_task)(); });
            return packed_task->get_future();
        }

        template <typename Function>
        CompleteAllJobsReturnType<Function> complete_all_jobs(size_t number_of_jobs, Function && function)
        {
            using ReturnType = CallableReturnType<Function, size_t>;

            std::vector<std::future<ReturnType>> futures;
            futures.reserve(number_of_jobs);

            for (size_t i = 0; i < number_of_jobs; ++i)
            {
                futures.emplace_back(this->add_job([function, i] { return function(i); }));
            }

            for (auto & future : futures)
            {
                if (future.valid())
                    future.wait();
            }

            if constexpr (!std::is_void_v<ReturnType>)
            {
                std::vector<ReturnType> results;
                results.reserve(number_of_jobs);
                for (auto & future : futures)
                {
                    // if the promise is broken, it will throw std::future_error
                    results.emplace_back(future.get());
                }
                return results;
            }
        }

        // stop all jobs from being processed then kill all threads
        void stop();
        // wait for all jobs to be read by threads, does not ensure the job is done though
        // you have to use future.wait() from add_job
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
