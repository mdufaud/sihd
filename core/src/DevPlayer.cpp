#include <sihd/core/DevPlayer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/ScopedModifier.hpp>
#include <sihd/util/Task.hpp>

#define CHANNEL_PLAY "play"
#define CHANNEL_END "end"

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(DevPlayer)

SIHD_LOGGER;

using namespace sihd::util;

DevPlayer::DevPlayer(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _waiting(false),
    _running(false),
    _channel_play_ptr(nullptr),
    _channel_end_ptr(nullptr)
{
    _runnable.set_method(this, &DevPlayer::_worker_loop);
    _worker.set_runnable(&_runnable);
    _collector.add_observer(this);
    this->set_provider_wait_time(10);
    this->set_scheduler_queue_size(200);
    this->add_conf("provider", &DevPlayer::set_provider);
    this->add_conf("provider_wait_time", &DevPlayer::set_provider_wait_time);
    this->add_conf("queue_size", &DevPlayer::set_scheduler_queue_size);
    this->add_conf("alias", &DevPlayer::add_alias);
}

DevPlayer::~DevPlayer()
{
}

bool    DevPlayer::is_running() const
{
    return _running;
}

bool    DevPlayer::set_scheduler_queue_size(size_t limit)
{
    _records_queue_limit = limit;
    return true;
}

bool    DevPlayer::set_provider(std::string_view path)
{
    _provider_path = path;
    return true;
}

bool    DevPlayer::set_provider_wait_time(time_t milliseconds)
{
    if (milliseconds <= 0)
    {
        SIHD_LOG(error, "DevPlayer: cannot wait for " << milliseconds << " milliseconds");
        return false;
    }
    _collector.set_timeout_milliseconds(milliseconds);
    return true;
}

bool    DevPlayer::add_alias(std::string_view alias_conf)
{
    sihd::util::Splitter splitter("=");
    std::vector<std::string> conf = splitter.split(alias_conf);

    if (conf.size() != 2)
    {
        SIHD_LOG(error, "DevReplayer: wrong alias configuration: '" << alias_conf
                << "' - expected RECORD_CHANNEL_NAME=CHANNEL_PATH");
        return false;
    }
    // conf[0] = record_channel_name
    // conf[1] = channel_path
    _map_channels_alias[conf[0]] = conf[1];
    return true;
}

void    DevPlayer::handle(sihd::core::Channel *c)
{
    if (c == _channel_play_ptr)
    {
        if (_channel_play_ptr->read<bool>(0) == false)
            _scheduler_ptr->pause();
        else
            _scheduler_ptr->resume();
    }
}

bool    DevPlayer::on_init()
{
    this->add_unlinked_channel(CHANNEL_PLAY, TYPE_BOOL, 1);
    this->add_unlinked_channel(CHANNEL_END, TYPE_BOOL, 1);
    _scheduler_ptr = this->add_child<Scheduler>("scheduler");
    return true;
}

bool    DevPlayer::on_start()
{
    // provider
    IProvider<PlayableRecord> *provider = this->find<IProvider<PlayableRecord>>(_provider_path);
    if (provider == nullptr)
    {
        SIHD_LOG(error, "DevReplayer: could not find provider: " << _provider_path);
        return false;
    }
    _collector.set_provider(provider);
    // channel play
    if (this->get_channel(CHANNEL_PLAY, &_channel_play_ptr) == false)
        return false;
    this->observe_channel(_channel_play_ptr);
    // channel end
    if (this->get_channel(CHANNEL_END, &_channel_end_ptr) == false)
        return false;
    _channel_end_ptr->write<bool>(0, false);
    // channels to play to
    Channel *channel_ptr;
    for (const auto & pair: _map_channels_alias)
    {
        channel_ptr = this->find<Channel>(pair.second);
        if (channel_ptr == nullptr)
        {
            SIHD_LOG(error, "DevRecorder: channel to record '"
                        << pair.first << "' not found: " << pair.second);
            return false;
        }
        _map_channels[pair.first] = channel_ptr;
    }
    // check if must play
    if (_channel_play_ptr->read<bool>(0) == false)
        _scheduler_ptr->pause();
    if (_scheduler_ptr->start() == false)
    {
        SIHD_LOG(error, "DevReplayer: could not start scheduler");
        return false;
    }
    // start thread
    if (_worker.start_worker(this->name()) == false)
    {
        _scheduler_ptr->stop();
        SIHD_LOG(error, "DevReplayer: could not start worker");
        return false;
    }
    _running = true;
    return true;
}

bool    DevPlayer::run()
{
    std::lock_guard l(_run_mutex);
    if (_running == false)
        return false;
    // get first record in queue and write record in channel
    PlayableRecord & record = _queue.front();
    _queue.pop();
    Channel *c = _map_channels[record.name];
    if (c != nullptr)
        c->write(*record.value);
    else
        SIHD_LOG(error, "DevPlayer: channel '" << record.name << "' not found");
    // notify worker loop that a record has been played
    _waitable.notify(1);
    // if no next record to be played - notify the end of player
    if (_last_record && _queue.empty())
        _channel_end_ptr->write<bool>(0, true);
    return true;
}

void    DevPlayer::handle(Collector<PlayableRecord> *collector)
{
    // called for each record with a lock on collector data
    PlayableRecord record = collector->data();
    _queue.push(record);
    _first_timestamp = record.timestamp * (1 * (_first_timestamp < 0));
    time_t execute_at = _time_begin + (record.timestamp - _first_timestamp);
    {
        ScopedModifier m(_waiting, true);
        // wait tasks to be played as not to overflow the scheduler
        while (_running && (_queue.size() > _records_queue_limit))
            _waitable.infinite_wait();
    }
    // calls DevPlayer::run to execute record at setted time
    if (_running)
        _scheduler_ptr->add_task(new Task(this, execute_at));
}

bool    DevPlayer::_worker_loop()
{
    _time_begin = _scheduler_ptr->now();
    _first_timestamp = -1;
    _last_record = false;
    // run collector loop which calls DevPlayer::handle for each record
    bool ret = _collector.run();
    // last record to be played in scheduler thread can have no next record
    _last_record = true;
    // if no task in queue - notify end of player
    if (_queue.empty())
        _channel_end_ptr->write<bool>(0, true);
    return ret;
}

bool    DevPlayer::on_stop()
{
    {
        std::lock_guard l(_run_mutex);
        _running = false;
    }
    while (_waiting.load())
        _waitable.notify_all();
    if (_worker.stop_worker() == false)
        SIHD_LOG(error, "DevReplayer: could not stop worker");
    if (_scheduler_ptr->stop() == false)
        SIHD_LOG(error, "DevReplayer: could not stop scheduler");
    _channel_play_ptr = nullptr;
    _channel_end_ptr = nullptr;
    _collector.wait_stop();
    _collector.set_provider(nullptr);
    _map_channels.clear();
    return true;
}

bool    DevPlayer::on_reset()
{
    _queue = std::queue<PlayableRecord>();
    _map_channels_alias.clear();
    return true;
}

}