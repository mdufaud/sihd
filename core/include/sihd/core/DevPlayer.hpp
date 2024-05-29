#ifndef __SIHD_CORE_DEVPLAYER_HPP__
#define __SIHD_CORE_DEVPLAYER_HPP__

#include <queue>

#include <sihd/util/Collector.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/IProvider.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Worker.hpp>

#include <sihd/core/DevRecorder.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/core/Records.hpp>

namespace sihd::core
{

class DevPlayer: public sihd::core::Device,
                 public sihd::util::IHandler<sihd::util::Collector<PlayableRecord> *>,
                 public sihd::util::IRunnable
{
    public:
        DevPlayer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevPlayer();

        bool is_running() const override;

        bool set_provider(std::string_view path);
        bool set_provider_wait_time(time_t milliseconds);
        bool add_alias(std::string_view alias_conf);
        bool set_scheduler_queue_size(size_t limit);

    protected:
        using Device::handle;

        void handle(sihd::core::Channel *c) override;
        void handle(sihd::util::Collector<PlayableRecord> *collector) override;

        bool run() override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        bool _main_loop();
        void _provider_ended();

        bool _running;
        std::atomic<bool> _last_record;
        size_t _records_queue_limit;
        std::string _provider_path;

        Channel *_channel_play_ptr;
        Channel *_channel_end_ptr;
        sihd::util::Scheduler *_scheduler_ptr;

        // channels to write records to
        std::map<std::string, Channel *> _map_channels;
        // channels to write configuration
        std::map<std::string, std::string> _map_channels_alias;

        std::optional<time_t> _first_timestamp;

        // queue to pass data between scheduler thread and worker thread
        sihd::util::SafeQueue<PlayableRecord> _safe_queue;
        // notifies scheduler task has played
        sihd::util::Waitable _waitable;
        // provider reading thread
        sihd::util::Worker _worker;
        // collects records from provider
        sihd::util::Collector<PlayableRecord> _collector;

        sihd::util::Runnable _runnable;
};

} // namespace sihd::core

#endif