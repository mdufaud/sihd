#ifndef __SIHD_CORE_DEVWORKERPOOL_HPP__
#define __SIHD_CORE_DEVWORKERPOOL_HPP__

#include <atomic>
#include <map>
#include <optional>

#include <sihd/core/DevWorker.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/ThreadPool.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::core
{

// the relays of DevWorker on a pool of threads, each relay keeping its order
class DevWorkerPool: public sihd::core::DevWorker
{
    public:
        DevWorkerPool(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevWorkerPool();

        bool is_running() const override;

        bool set_threads(size_t threads);

    protected:
        using sihd::core::DevWorker::handle;

        void handle(sihd::core::Channel *c) override;

        bool on_start() override;
        bool on_stop() override;

    private:
        // the writes of one relay, written by one job at a time so they keep their order
        struct Lane
        {
                sihd::util::SafeQueue<sihd::util::ArrByte> pending;
                // the job runs while it is not zero
                std::atomic<size_t> unwritten = 0;
        };

        void _submit(Channel *out, Lane & lane);
        void _relay_one(Channel *out, Lane & lane);

        size_t _threads;
        std::map<Channel *, Lane> _lanes;
        std::optional<sihd::util::ThreadPool> _pool;

        sihd::util::Waitable _idle;
        size_t _in_flight;
};

} // namespace sihd::core

#endif
