#ifndef __SIHD_CORE_DEVPLAYER_HPP__
# define __SIHD_CORE_DEVPLAYER_HPP__

# include <sihd/core/Device.hpp>
# include <sihd/core/DevRecorder.hpp>
# include <sihd/core/Records.hpp>
# include <sihd/util/Scheduler.hpp>
# include <sihd/util/Worker.hpp>
# include <sihd/util/IProvider.hpp>

# include <queue>

namespace sihd::core
{

class DevPlayer:    public sihd::core::Device,
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
        void handle([[maybe_unused]] sihd::core::Channel *c) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        bool _worker_loop();

        bool _running;
        bool _last_record;

        std::mutex _run_mutex;
        size_t _records_queue_limit;
        time_t _provider_wait_milliseconds;

        std::string _provider_path;
        sihd::util::IProvider<PlayableRecord &> *_provider_ptr;
        std::queue<PlayableRecord> _queue;

        std::map<std::string, Channel *> _map_channels;
        std::map<std::string, std::string> _map_channels_alias;
        Channel *_channel_play_ptr;
        Channel *_channel_end_ptr;

        sihd::util::Scheduler *_scheduler_ptr;
        sihd::util::Worker _worker;
        sihd::util::Waitable _waitable;
};

}

#endif