#ifndef __SIHD_CORE_DEVPLAYER_HPP__
# define __SIHD_CORE_DEVPLAYER_HPP__

# include <sihd/core/Device.hpp>
# include <sihd/core/DevRecorder.hpp>
# include <sihd/core/Records.hpp>
# include <sihd/util/Scheduler.hpp>
# include <sihd/util/Worker.hpp>
# include <sihd/util/IProvider.hpp>
# include <sihd/util/Collector.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/Runnable.hpp>

# include <queue>

namespace sihd::core
{

class DevPlayer:    public sihd::core::Device,
                    public sihd::util::IHandler<sihd::util::Collector<PlayableRecord> *>,
                    public sihd::util::IRunnable
{
    public:
        DevPlayer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevPlayer();

        bool run() override;
        bool is_running() const override;

        bool set_provider(const std::string & path);
        bool set_provider_wait_time(time_t milliseconds);
        bool add_alias(const std::string & alias_conf);
        bool set_scheduler_queue_size(size_t limit);

    protected:
        using Device::handle;

        void handle(sihd::core::Channel *c) override;
        void handle(sihd::util::Collector<PlayableRecord> *collector) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        bool _worker_loop();

        bool _running;
        bool _last_record;
        size_t _records_queue_limit;
        std::string _provider_path;

        std::mutex _run_mutex;

        Channel *_channel_play_ptr;
        Channel *_channel_end_ptr;
        sihd::util::Scheduler *_scheduler_ptr;

        // channels to write records to
        std::map<std::string, Channel *> _map_channels;
        // channels to write configuration
        std::map<std::string, std::string> _map_channels_alias;

        time_t _time_begin;
        time_t _first_timestamp;

        // queue to pass data between scheduler thread and worker thread
        std::queue<PlayableRecord> _queue;
        // notifies scheduler task has played
        sihd::util::Waitable _waitable;
        // provider reading thread
        sihd::util::Worker _worker;
        // collects records from provider
        sihd::util::Collector<PlayableRecord> _collector;

        sihd::util::Runnable _runnable;
};

}

#endif