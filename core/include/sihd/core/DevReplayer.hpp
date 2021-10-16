#ifndef __SIHD_CORE_DEVREPLAYER_HPP__
# define __SIHD_CORE_DEVREPLAYER_HPP__

# include <sihd/core/Device.hpp>
# include <sihd/core/DevRecorder.hpp>
# include <sihd/util/Scheduler.hpp>
# include <sihd/util/Worker.hpp>

namespace sihd::core
{

class DevReplayer:  public sihd::core::Device,
                    public sihd::util::IRunnable
{
    public:
        struct PlayableRecord
        {
            std::string channel;
            time_t timestamp = 0;
            sihd::util::IArray *value = nullptr;
        };
        typedef std::vector<PlayableRecord> replay_list;

        DevReplayer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevReplayer();

        bool run() override;
        bool is_running() const override;

        bool add_alias(const std::string & alias_conf);
        bool set_csv(const std::string & path);

    protected:
        void observable_changed([[maybe_unused]] sihd::core::Channel *c) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        bool _loop();

        bool _running;

        sihd::util::SteadyClock _clock;
        time_t _started_nano;
        size_t _idx_playing_records;
        size_t _records_to_play;
        size_t _records_queue_limit;
        std::string _csv_path;
        replay_list _playable_records;

        std::map<std::string, Channel *> _map_channels;
        std::map<std::string, std::string> _map_channels_alias;
        Channel *_channel_play_ptr;

        sihd::util::Scheduler _scheduler;
        sihd::util::Worker _worker;
        sihd::util::Waitable _waitable;
};

}

#endif