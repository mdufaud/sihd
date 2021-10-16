#include <sihd/core/DevReplayer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/Task.hpp>

#define CHANNEL_PLAY "play"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevReplayer)

LOGGER;

DevReplayer::DevReplayer(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _running(false),
    _channel_play_ptr(nullptr),
    _scheduler("scheduler", this)
{
    _idx_playing_records = 0;
    _records_to_play = 0;
    _records_queue_limit = 20;
    _worker.set_runnable(new sihd::util::Task([this] () -> bool
    {
        return this->_loop();
    }));
}

DevReplayer::~DevReplayer()
{
}

bool    DevReplayer::is_running() const
{
    return _running;
}

bool    DevReplayer::add_alias(const std::string & alias_conf)
{
    std::vector<std::string> conf = sihd::util::Str::split(alias_conf, "=");

    if (conf.size() != 2)
    {
        LOG(error, "DevReplayer: wrong alias configuration: '" << alias_conf
                << "' - expected RECORD_CHANNEL_NAME=CHANNEL_PATH");
        return false;
    }
    // conf[0] = record_channel_name
    // conf[1] = channel_path
    _map_channels_alias[conf[0]] = conf[1];
    return true;
}

bool    DevReplayer::set_csv(const std::string & path)
{
    if (!sihd::util::Files::exists(path))
    {
        LOG(error, "DevReplayer: csv configuration path does not exists: " << path);
        return false;
    }
    _csv_path = path;
    return true;
}

void    DevReplayer::observable_changed(sihd::core::Channel *c)
{
    if (c == _channel_play_ptr)
    {
        if (_channel_play_ptr->read<bool>(0) == false)
            _scheduler.pause();
        else
            _scheduler.resume();
    }
}

bool    DevReplayer::on_init()
{
    this->add_unlinked_channel(CHANNEL_PLAY, sihd::util::DBOOL, 1);
    return true;
}

bool    DevReplayer::on_start()
{
    _idx_playing_records = 0;
    _records_to_play = 0;
    if (this->get_channel(CHANNEL_PLAY, &_channel_play_ptr) == false)
        return false;
    this->observe_channel(_channel_play_ptr);

    _started_nano = _clock.now();
    if (_channel_play_ptr->read<bool>(0) == false)
        _scheduler.pause();
    if (_scheduler.start() == false)
    {
        LOG(error, "DevReplayer: could not start scheduler");
        return false;
    }
    if (_worker.start_worker(this->get_name()) == false)
    {
        _scheduler.stop();
        LOG(error, "DevReplayer: could not start worker");
        return false;
    }
    _running = true;
    return true;
}

bool    DevReplayer::run()
{
    PlayableRecord & record = _playable_records[_idx_playing_records];
    Channel *c = _map_channels[record.channel];
    if (c != nullptr)
        c->write(*record.value);
    ++_idx_playing_records;
    --_records_to_play;
    _waitable.notify(1);
    return true;
}

bool    DevReplayer::_loop()
{
    time_t begin = _scheduler.get_clock()->now();
    time_t first = _playable_records[0].timestamp;
    time_t execute_at;
    for (const PlayableRecord & record: _playable_records)
    {
        execute_at = begin + (first - record.timestamp);
        while (_running && (_records_to_play >= _records_queue_limit))
            _waitable.infinite_wait();
        if (_running == false)
            return false;
        _scheduler.add_task(new sihd::util::Task(this, execute_at));
        ++_records_to_play;
    }
    return true;
}

bool    DevReplayer::on_stop()
{
    _running = false;
    _waitable.notify_all();
    if (_worker.stop_worker() == false)
        LOG(error, "DevReplayer: could not stop worker");
    if (_scheduler.stop() == false)
        LOG(error, "DevReplayer: could not stop scheduler");
    _map_channels.clear();
    return true;
}

bool    DevReplayer::on_reset()
{
    _map_channels_alias.clear();
    _playable_records.clear(); // TODO
    return true;
}

}